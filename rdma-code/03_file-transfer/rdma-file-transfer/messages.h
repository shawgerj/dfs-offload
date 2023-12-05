#ifndef RDMA_MESSAGES_H
#define RDMA_MESSAGES_H

const char *DEFAULT_PORT = "12345";
const size_t BUFFER_SIZE = 10 * 1024 * 1024;

enum message_id
{
  MSG_INVALID = 0,
  MSG_MR,
  MSG_READY,
  MSG_DONE,
  MSG_ACK
};

struct message
{
  int id;

  union
  {
    struct
    {
      uint64_t addr;
      uint32_t rkey;
    } mr;
  } data;
};

struct name_header
{
  uint32_t namelen;
  uint32_t offset;
};

#endif
