| Sender -> Receiver   | Command                                            | Message                               | Receiver Log                                                         |
| -------------------- | -------------------------------------------------- | ------------------------------------- | -------------------------------------------------------------------- |
| PD                   | ./PD sigma02 -d 57000 -n sigma03 -p 58000 58000    |                                       |                                                                      |
| AS                   | ./AS -p 58000 -v                                   |                                       |                                                                      |
| PD->AS               | reg 12345 password                                 | REG 12345 password sigma02 57000\n    | PD: new user, UID=12345                                              |
| AS->PD               |                                                    | RRG OK\n                              | Registration successful                                              |
| User                 | ./User -n sigma03 -p 58000 -m sigma04 -q 59000     |                                       |                                                                      |
| User->AS             | login 12345 password                               | LOG 12345 password\n                  | User: login ok, UID=12345                                            |
| AS->User             |                                                    | RLO OK\n                              | You are now logged in                                                |
| User->AS             | req U f1.txt                                       | REQ 12345 7654 U f1.txt\n             | User: upload req, UID=12345 <br> file: f1.txt, RID=7654 <br> VC=9987 |
| AS->PD               |                                                    | VLC 12345 9987 U f1.txt\n             | VC=9987, upload: f1.txt                                              |
| PD->AS               |                                                    | RVC OK\n                              |                                                                      |
| AS->User             |                                                    | RRQ OK\n                              |                                                                      |
| User->AS             | val 9987                                           | AUT 12345 7654 9987\n                 | User: UID=12345 <br> U, f1.txt, TID=2020                             |
| AS->User             |                                                    | RAU 2020\n                            | Authenticated! (TID=2020)                                            |
| FS                   | ./FS -p 59000 -v                                   |                                       |                                                                      |
| User->FS             | upload f1.txt                                      | UPL 12345 2020 f1.txt 6 Hello!\n      | UID=12345: upload f1.txt (6 Bytes)                                   |
| FS->AS               |                                                    | VLD 12345 2020\n                      | User: UID=12345 U, f1.txt, TID=2020                                  |
| AS->FS               |                                                    | CNF 12345 2020 U f1.txt\n             | operation validated                                                  |
| FS                   | \<executes operation\>                             |                                       | f1.txt stored for UID=12345                                          |
| FS->User             |                                                    | RUP OK\n                              | success uploading f1.txt                                             |
| User                 | exit                                               |                                       |                                                                      |
|                      |                                                    |                                       |                                                                      |
|                      |                                                    |                                       |                                                                      |
|                      |                                                    |                                       |                                                                      |
|                      |                                                    |                                       |                                                                      |
|                      |                                                    |                                       |                                                                      |
|                      |                                                    |                                       |                                                                      |
|                      |                                                    |                                       |                                                                      |
|                      |                                                    |                                       |                                                                      |
