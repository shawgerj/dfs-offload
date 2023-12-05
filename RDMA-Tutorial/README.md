# RDMA-Tutorial
This project presents an example based tutorial on RDMA based programming. A more detailed 
discussion can be found on the [Wiki](https://github.com/jcxue/RDMA-Tutorial/wiki) page.

## Hardware and software requirement
 * Mellanox HCAs
 * GNU make
 * gcc-4.4
 * Mellanox OFED 3.3

## How to use

### build project
Simply use ```make``` to build the release version or ```make debug``` to build the 
debug version.

### navigate through examples
The project contains 4 examples. Details of the examples can be found on the 
[Wiki](https://github.com/jcxue/RDMA-Tutorial/wiki) page. The code of the examples
are organized as git commits. Simply do ```git log --oneline``` to find the commit version number 
corresponding to the examples you are looking for.

## IBV Pingpong
### Server
ibv_rc_pingpong -g 1 -d mlx5_2 -i 1

### Client
ibv_rc_pingpong -g 1 -d mlx5_2 -i 1 node1

## Contact

Jiachen Xue (jcxue.work@gmail.com)
John Shawger (shawgerj@cs.wisc.edu)
