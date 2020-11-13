#define TRUE 1
#define FALSE 0
#define STDIN 0

//IP address of the machine where the AS runs
#define ASIP "localhost"
// well-known port where the AS accepts requests
// 58000 + GN --> 58022
#define ASPORT "58022"

#define PDIP "localhost"
// well-known port where the PD runs an UDP server to accept future AS messages with verification codes
// 57000 + GN --> 57022
#define PDPORT "57022"

// IP address of the machine where the file server (FS)
#define FSIP "localhost"
// well-known TCP port where the FS server accepts requests
// 59000 + GN --> 59022
#define FSPORT "59000"