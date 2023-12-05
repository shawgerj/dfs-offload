#include <fcntl.h>
#include <libgen.h>

#include "common.h"
#include "messages.h"

struct client_context
{
  char *buffer;
  struct ibv_mr *buffer_mr;

  struct message *msg;
  struct ibv_mr *msg_mr;

  uint64_t peer_addr;
  uint32_t peer_rkey;

  int fd;
  const char *file_name;
};

static void write_remote(struct rdma_cm_id *id, uint32_t len, uint32_t offset)
{
  struct client_context *ctx = (struct client_context *)id->context;

  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)id;
  wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
  wr.send_flags = IBV_SEND_SIGNALED;
  if (offset) {
    wr.imm_data = htonl(offset);
  } else {
    wr.imm_data = htonl(len);
  }
  wr.wr.rdma.remote_addr = ctx->peer_addr + offset;
  wr.wr.rdma.rkey = ctx->peer_rkey;

  if (len) {
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)ctx->buffer;
    sge.length = len;
    sge.lkey = ctx->buffer_mr->lkey;
  }

  TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

static void write_remote_one_sided(struct rdma_cm_id *id, uint32_t len, uint32_t offset)
{
  struct client_context *ctx = (struct client_context *)id->context;

  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge = {
    .addr = (uintptr_t)ctx->buffer,
    .length = len,   
    .lkey = ctx->buffer_mr->lkey,
  };

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)id;
  wr.opcode = IBV_WR_RDMA_WRITE;
  // not needed for now
  //  wr.send_flags = IBV_SEND_SIGNALED; 
  wr.wr.rdma.remote_addr = ctx->peer_addr;
  wr.wr.rdma.rkey = ctx->peer_rkey;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  
  TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));

  // do I need to poll on cq_send here?
}

static void post_receive(struct rdma_cm_id *id)
{
  struct client_context *ctx = (struct client_context *)id->context;

  struct ibv_recv_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)id;
  wr.sg_list = &sge;
  wr.num_sge = 1;

  sge.addr = (uintptr_t)ctx->msg;
  sge.length = sizeof(*ctx->msg);
  sge.lkey = ctx->msg_mr->lkey;

  TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

static void send_next_chunk(struct rdma_cm_id *id)
{
  struct client_context *ctx = (struct client_context *)id->context;

  ssize_t size = 0;

  size = read(ctx->fd, ctx->buffer, BUFFER_SIZE);

  if (size == -1)
    rc_die("read() failed\n");

  write_remote(id, size, 0);
}

static size_t send_one_sided(struct rdma_cm_id *id)
{
  struct client_context *ctx = (struct client_context *)id->context;

  ssize_t size = 0;

  size = read(ctx->fd, ctx->buffer, BUFFER_SIZE);

  if (size == -1)
    rc_die("read() failed\n");

  write_remote_one_sided(id, size, 0);
  return size;
  
}

// size: the size of the file that was just sent, so we know where
// to put the file name in the server buffer
static void send_file_name(struct rdma_cm_id *id, size_t size)
{
  struct client_context *ctx = (struct client_context *)id->context;
  uint32_t namelen = strlen(ctx->file_name) + 1;

  memcpy(ctx->buffer, namelen, sizeof(uint32_t));
  strcpy(ctx->buffer + sizeof(uint32_t), ctx->file_name);

  write_remote(id, strlen(ctx->file_name) + 1 + sizeof(uint32_t), size);
}

static void on_pre_conn(struct rdma_cm_id *id)
{
  struct client_context *ctx = (struct client_context *)id->context;

  posix_memalign((void **)&ctx->buffer, sysconf(_SC_PAGESIZE), BUFFER_SIZE);
  TEST_Z(ctx->buffer_mr = ibv_reg_mr(rc_get_pd(), ctx->buffer, BUFFER_SIZE, 0));

  posix_memalign((void **)&ctx->msg, sysconf(_SC_PAGESIZE), sizeof(*ctx->msg));
  TEST_Z(ctx->msg_mr = ibv_reg_mr(rc_get_pd(), ctx->msg, sizeof(*ctx->msg), IBV_ACCESS_LOCAL_WRITE));

  post_receive(id);
}

static void on_completion(struct ibv_wc *wc)
{
  printf("client: on_completion\n");
  struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)(wc->wr_id);
  struct client_context *ctx = (struct client_context *)id->context;

  if (wc->opcode & IBV_WC_RECV) {
    if (ctx->msg->id == MSG_MR) {
      ctx->peer_addr = ctx->msg->data.mr.addr;
      ctx->peer_rkey = ctx->msg->data.mr.rkey;

      printf("received MR, sending file via one-sided RDMA\n");
      size_t size = send_one_sided(id);
      // note that we can send the file name and these operations
      // are guaranteed to arrive in order on the server without
      // explicitly waiting for the one-sided RDMA completion.
      // This will NOT be true if we send the confirmation to a
      // different machine, e.g. SNIC
      printf("finished one-sided RDMA, sending file name\n");
      send_file_name(id, size);
    } else if (ctx->msg->id == MSG_READY) {
      printf("received READY, sending chunk\n");
      send_next_chunk(id);
    } else if (ctx->msg->id == MSG_DONE) {
      printf("received DONE, disconnecting\n");
      rc_disconnect(id);
      return;
    }

    post_receive(id);
  }
}

int main(int argc, char **argv)
{
  struct client_context ctx;

  if (argc != 3) {
    fprintf(stderr, "usage: %s <server-address> <file-name>\n", argv[0]);
    return 1;
  }

  ctx.file_name = basename(argv[2]);
  ctx.fd = open(argv[2], O_RDONLY);

  if (ctx.fd == -1) {
    fprintf(stderr, "unable to open input file \"%s\"\n", ctx.file_name);
    return 1;
  }

  rc_init(
    on_pre_conn,
    NULL, // on connect
    on_completion,
    NULL); // on disconnect

  rc_client_loop(argv[1], DEFAULT_PORT, &ctx);

  close(ctx.fd);

  return 0;
}

