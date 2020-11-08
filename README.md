# RC Two-Factor Authentication

#### Index
- [Introduction](#introduction)
    - [Full Operation](#full-operation)
- [Project Specification](#project-specification)
    - [AS](#authentication-server)
    - [FS](#file-server)
    - [PD](#personal-device-application)
    - [User](#user-application)
- [Communication Protocols Specification](#communication-protocols-specification)
    - [PD-AS](#pd-as-udp)
    - [User-AS](#user-as-tcp)
    - [AS-FS](#as-fs-udp)
    - [User-FS](#user-fs-tcp)
- [Project Submission](#project-submission)
- [Checklist](#checklist)
- [Questions](#questions)
- [TODO](#todo)
---
## Introduction
Password + code sent to personal device

The development:

1. [Authentication Server (AS)](#authentication-server)
2. [File Server (FS)](#file-server)
3. [Personal Device Application (PD)](#personal-device-application) (one per **user** at a given time)
4. [User Application (User)](#user-application) (several instances per **user**)

*AS* and *FS* will be running on machines with known IP addresses and ports.

The **user** (a person) has simultaneously access to *PD* and *User*

Client-server paradigm, using transport layer services made available by the socket interface.

Interactions *AS*-*PD* and *AS*-*FS* use `UDP protocol`. All other interactions use `TCP protocol`

The **user**, after completing the two-factor authentication procedure, can ask to make one of the possible operations (*Fop*):
- list (`L`) of all the **user**'s files
- upload (`U`)
- retrieve (`R`)
- delete (`D`) a file
- remove (`X`) the **user** information from the *AS* database, deleting all its files from the *FS*

For any single transaction with the *FS*, the **user** must first request a new transaction ID (*TID*) to the *AS*

---
### Full operation:
1. <details><summary>Registration with the <i>AS</i> using the <i>PD</i> - <i>UID</i> and <i>pass</i> needed</summary>

    **User** registers with the *AS* using the *PD*, providing a *UID* (`5 digit number` user id) and a *pass* (`8 alphanumeric characters` password). At each registration, the *PD* sends it IP and UDP port for the AS to be able to contact back to the *PD*
    

    First registation defines the pair *UID/pass* at the AS database: check if the *UID* already exists (and if the given password is correct) or adds the pair to the database
</details>

2. <details><summary>Sending the <b>user</b>'s <i>UID</i>, <i>Fop</i> and in some cases the <i>Fname</i></summary>

    For any single transaction with the *FS*, the **user** must first request a new *TID* (`4 digit number` transaction ID) from the *AS*, sending its *UID* and the type of operation to perform. 

    If the *Fop* is `R`, `U` or `D` the **user** also sends to the *AS* the *Fname* (name of the file). The information is stored in its database and sends to the **user**'s *PD* a *VC* (`4 digit number` validation code), information about the *Fop* and the *Fname* (when it's given).
    
</details>

3. The **user** reads this *VC* at the *PD* and inserts it at the *User* application to be sent to the *AS* again.

4. The *AS* checks the *VC*, generates *TID*, stores the information for posterior validation of *FS* operations and replies to the *User* with the *TID*. 
    > The **user** may be informed of the *TID* but he will not use it directly at the keyboard

5. After receiving the *TID*, the *User* Application can complete the operation with the *FS* - identifying the messages with the *UID* and *TID*.

6. The *FS* validates an operation request with the *AS*, sending it the *UID* and the *TID*
7. The *AS* sends a message to the *FS* with the *UID* + *TID* + *Fop* + *Fname* (when given)
    > If during the validation *Fop* = X, the *AS* removes the *UID/pass* and confirms that the *FS* can remove all the files and directories associated with the user
8. <details><summary>The <i>FS</i> executes the <i>Fop</i></summary>

    - list (`L`): return the list of files already uploaded by this **user**
        > from this or other instances of the *User* application
    - upload (`U`): transmitting the selected file to the *FS*, which confirms the success (or not) of the upload
        > each user can have a maximum of 15 files sotred in the *FS* server
    - retrieve (`R`): send the requested file, if possible
    - delete (`D`): remove the identified file from the server

    
</details>

---
---
## Project Specification

---
### Authentication Server

#### Specification:
The program implementing the *AS* application should be invoked using the command:

       ./AS [-p ASport] [-v]

<details><summary>Legend</summary>

- `ASport` is the well-known port where the AS server accepts requests, both in UDP and TCP. This is an optional argument. If omitted, it assumes the value 58000+GN, where GN is the number of the group.
   
</details>

The *AS* makes available two server applications, one in `UDP` and the other in `TCP`, both with well-known port ASport, to answer requests from the *PD* and the *FS* (in `UDP`), and the *User* (in `TCP`) applications.
If the option *v* is set when invoking the program, it operates in *verbose* mode, meaning that the *AS* server outputs to the screen a short description of the received requests and the IP and port originating those requests.
Each received request should start being processed once it is received. 

---
### File Server

#### Specification:
The program implementing the *FS* application should be invoked using the command:

        ./FS [-p FSport] [-v]

<details><summary>Legend</summary>

- `FSport` is the well-known TCP port where the FS server accepts requests. This is an optional argument and, if omitted, assumes the value 59000+GN, where GN is the number of the group.
   
</details>

The *FS* server accepts `TCP` requests on the well-known port *FSport*, to answer *User* requests.
If the option *v* is set when invoking the program, it operates in *verbose* mode, meaning that the FS server outputs to the screen a short description of the received requests and the IP and port originating those requests.
Each received request should start being processed once it is received. 

---
### Personal Device Application

#### Operations:
- register in *AS* the personal devide of the **user** with a *UID* and *pass*
- receive 4-digit *VC* from the *AS*, which the *User* applications instances of this **user** can copy, to be used as a second authentication factor
- unregister as the personal device of the **user**, when exiting the *PD* application

#### Specification:
The program implementing the *PD* application should be invoked using the command:

        ./pd PDIP [-d PDport] [-n ASIP] [-p ASport]

<details><summary>Legend</summary>

- `PDIP` this is the IP address of this machine, where the personal device (PD) runs, specified in the dot-decimal notation.

- `PDport` this is the well-known port where the PD runs an UDP server to accept future AS messages with verification codes, as part of the two-factor authentication mechanism. This is an optional argument. If omitted, it assumes the value 57000+GN, where GN is the group number.

- `ASIP` this is the IP address of the machine where the authentication server (AS) runs. This is an optional argument. If this argument is omitted, the AS should be running on the same machine.

- `ASport` this is the well-known UDP port where the AS server accepts requests. This is an optional argument. If omitted, it assumes the value 58000+GN, where GN is the group number.
   
</details>

Once the *PD* program is running, it waits for a registration command from the **user**. 
Then it waits for validation codes (*VC*) sent by the *AS*, which should be displayed. 
The *PD* application can also receive a command to exit, unregistering the user.

#### Commands:
- [ ] `reg UID pass` – following this command the PD application sends a registration message to the AS server, using the UDP protocol, sending the user’s identification UID (the 5-digit IST student number) and the selected password (pass), consisting of 8 alphanumerical characters, restricted to letters and numbers. It also sends the IP PDIP and port PDport of the PD’s UDP server, so that the AS can later send verification codes to the PD.
The UID is stored in memory for the session duration. The result of the AS registration should be displayed to the user.
- [ ] `exit` – the PD application terminates, after unregistering with the AS.

---
### User Application

#### Operations:
- login (first authentication factor)
- request to the *AS* a 4-digit *TID*
- confirm the previous request by sending the *VC*, obtained from the *PD*, to the *AS* (second autthentication factor)
- after retrieving the *TID*, request the *Fop* to the *FS*
- display the received replies from the issued requests
- exit (terminating this instance of tue *User* application)

#### Specification:
The program implementing the *User* application should be invoked using the command:

        ./user [-n ASIP] [-p ASport] [-m FSIP] [-q FSport]

<details><summary>Legend</summary>

- `ASIP` this is the IP address of the machine where the authentication server (AS) runs. This is an optional argument. If this argument is omitted, the AS should be running on the same machine.
- `ASport` this is the well-known TCP port where the AS server accepts requests. This is an optional argument. If omitted, it assumes the value 58000+GN, where GN is the group number. 
- `FSIP` this is the IP address of the machine where the file server (FS) runs. This is an optional argument which, if omitted means the FS is running on the same machine.
- `FSport` this is the well-known TCP port where the FS server accepts requests. This is an optional argument. If omitted, it assumes the value 59000+GN, where GN is the group number.
   
</details>

Once the *User* application program is running, it establishes a **TCP session** with the *AS*, which remains open, and then waits for the **user** to indicate the action to take using one of the possible commands.

#### Commands:
- [ ] `login UID pass` – after this command the User application sends to the AS the user’s ID UID (5-digit IST student number) and a password pass (8 alphanumerical characters, restricted to letters and numbers), for validation by the AS. The result of the AS validation should be displayed to the user. The TCP session remains open and the UID + pass are locally stored in memory for the session duration.
- [ ] `req Fop [Fname]` – following this command the User sends a message to the AS requesting a transaction ID code (TID). This request message includes the UID and the type of file operation desired (Fop), either list (L), retrieve (R), upload (U), delete (D) or remove (X), and if appropriate (when Fop is R, U or D) also sends the Fname. The user should then check the PD and wait for a validation code (VC) to arrive.
- [ ] `val VC` – after checking the VC on the PD the user issues this command, sending a message to the AS with the VC. In reply the AS should confirm (or not) the success of the two-factor authentication, which should be displayed. The AS also sends the transaction ID TID. Now the user can perform the desired file operation with the FS.
- [ ] `list` or `l`– following this command the User application establishes a TCP session with the FS server, asking for the list of files this user has previously uploaded to the server. The message includes the UID, the TID and the type of file operation desired (Fop). The reply should be displayed as a numbered list of filenames and the respective sizes.
- [ ] `retrieve filename` or `r filename` – following this command the User application establishes a TCP session with the FS server, to retrieve the selected file filename. The message includes the UID, the TID, the Fop and Fname. The confirmation of successful transmission (or not) should be displayed.
- [ ] `upload filename` or `u filename` – following this command the User application establishes a TCP session with the FS server, to upload the file filename. The message includes the UID, the TID, the Fop, Fname and the file size. The confirmation of successful transmission (or not) should be displayed.
- [ ] `delete filename` or `d filename` – following this command the User application establishes a TCP session with the FS server, to delete the file filename. The message includes the UID, the TID, the Fop and Fname. The confirmation of successful deletion (or not) should be displayed.
- [ ] `remove` or `x` – this command is used to request the FS to remove all files and directories of this User, as well as to request the FS to instruct the AS to delete this user’s login information. The result of the command should be displayed to the user. The User application then closes all TCP connections and terminates.
- [ ] `exit` – the User application terminates after closing any open TCP connections.

---
---
## Communication Protocols Specification

---
### PD-AS UDP
Request and reply messages:
- [ ] `REG UID pass PDIP PDport` - registration
- [ ] `RRG status` - check if registration successful

- [ ] `VLC UID VC Fop [Fname]` - after the request for the second factor authentication, the *AS* sends to the *PD* this information 
    - this information should be displayed by the *PD* application

- [ ] `RVC status` - reply to receiving the *VC* with `OK` or `NOK`
- [ ] `UNR UID pass` - following the `exit` command, the *PD* asks the *AS* to unregister this **user**
- [ ] `RUN status` - check if unresgistration successful

> If an unexpected protocol message is received, the reply is `ERR`

> In the above messages the separation between any two items consists of a single space and each request or reply message ends with the character “\n”.

---
### User-AS TCP
Request and reply messages:
- [ ] `LOG UID pass` - following the login command, the *User* sends the *AS* server a message with *UID* and *pass* for validation
- [ ] `RLO status` - check if login successful
- [ ] `REQ UID RID Fop [Fname]` - after the `req` command, request sent to *AS* to perform the *Fop*. This message initiates the second factor authentication procedure 
- [ ] `RRQ status`
- [ ] `AUT UID RID VC` - after checking the *VC* on the *PD*, the *User* sends this message to *AS* to complete 2FA
- [ ] `RAU TID` - *AS* confirms or not the success of 2FA

> If an unexpected protocol message is received, the reply is `ERR`

> In the above messages the separation between any two items consists of a single space and each request or reply message ends with the character “\n”.

---
### AS-FS UDP
Request and reply messages:
- [ ] `VLD UID TID` - *FS* validates operation with *AS*
- [ ] `CNF UID TID Fop [Fname]` - reply to the `VLD` message

> If an unexpected protocol message is received, the reply is `ERR`

> In the above messages the separation between any two items consists of a single space and each request or reply message ends with the character “\n”.

---
### User-FS TCP
Request and reply messages:
- [ ] `LST UID TID` - request from *User* to listing files
- [ ] `RLS N[ Fname Fsize]*` - listed files from *FS* to *User*
- [ ] `RTV UID TID Fname` - request from *User* to retrieving file
- [ ] `RRT status [Fsize data]` - retrieved file from *FS* to *User*
- [ ] `UPL UID TID Fname Fsize data` - request from *User* to upload file
- [ ] `RUP status` - success of upload message from *FS* to *User*
- [ ] `DEL UID TID Fname` - request from *User* to delete file
- [ ] `RDL status` - success of delete message from *FS* to *User*
- [ ] `REM UID TID` - request from *User* to remove account
- [ ] `RRM status` - success of remove message from *FS* to *User*

> If an unexpected protocol message is received, the reply is `ERR`

> In the above messages the separation between any two items consists of a single space and each request or reply message ends with the character “\n”.

---
---
## Project Submission
- include the programs implementing the *User*, the *PD*, the *AS* server and the *FS* server
- include de *Makefile*, which should compile the code and place the executables in the current directory with the `make` command
- include auxiliary files needed for the project, with a readme.txt file
- create a *zip* archive with all source code, makefile and auxiliary files
- name of zip: `proj_22.zip`

> submition by email until **November 13, at 23:59 PM**

---
---
## CHECKLIST
- implement verbose as
- SIGPIPE (não deixar que o servidor morra quando tenta fazer um write() e não consegue falar com o cliente)
- SIGALARM para quando a resposta demora demasiado tempo a chegar)
- SIGINT para quando se carrega no ctrl+c o programa terminar ordeiramente
- fix sendto sending too many chars
- close connections on exit
- reads dentro de um ciclo para garantir que tudo foi lido (e os writes?) --> verificar que foram lidos menos do que o número de bytes possivel de ler no read. Se não, ver se terminou num '\n' e se não significa que há mais por ler
- quando há uma falha num comando, deve-se terminar ordeiramente
- no read, n=0 significa que a sessão fechou

---
---
## Questions:
- terminar ordeiramente: só nos clientes, quando recebem uma resposta estranha do servidor e no servidor enviar apenas um "ERR" de volta ou terminar também no caso de ser um servidor? 
> no caso do AS e do FS fechar apenas a ligação correspondente
- quando terminamos o PD com ctrl+c, é suposto o PD fazer unregister com o AS certo? Ou seja, é chamar a funcao unregistration()?
> sim
- o que acontece se o PD fizer unregistration() e o as mandar um NOK?
> fazemos um print no pd a informar que houve um erro, mas terminamos na mesma
- registration no PD: é suposto termos um timer, depois de enviarmos a mensagem e se passado x segundos não obtivermos resposta do AS, voltar a mandar o pedido? Fazer um print no terminal a avisar o PD que não foi possível fazer o registo? 
> ter um timer, voltar a enviar. Se enviar várias vezes escrever no terminal que não foi possível comunicar com o servidor e morrer
- o que é suposto acontecer quando o AS recebe um RVC UID OK e RVC UID NOK?
- quando é enviamos um RRQ NOK? Quando o uid não corresponde a um PD registado? ou quando o AS não consegue enviar o VC ao PD?
- numa ligação tcp, definimos o numero maximo de clientes que se podem estar a tentar ligar ao servidor ao mesmo tempo (na funcao listen). Como devemos tratar o caso em que um user se tenta ligar por tcp mas não consegue? Voltar a tentar? Matar o user?
> pressupor que tal não acontece. Se o user não se conseguir ligar, morre e é a própria pessoa que 

#### Novas questões:
- confirmar que o PD só pode ter um uid e pass (e portanto que o AS deve devolver NOK se o PD se tenta registar com um uid e pass diferentes)
- quando se faz uma unregistration é suposto apagar mesmo o registo?
- um PD tem de ser capaz de servir vários utilizadores?
- podemos assumir um valor máximo para o Fsize? 

---
---
## TODO:
- verificar que os ficheiros estao sempre a terminar como deve de ser e que os servidores nunca morrem quando recebem algo inválido [**Susana**]
- resend de mensagens no AS (e no FS) [**Susana**]
- informar no terminal do PD quando morre de aborrecimento
- um user pode ter que estabelecer várias ligações tcp com um fs ao mesmo tempo? Ou é suposto esperar por ler tudo o que vem do fs antes de permitir uma nova operação?

- PD
- AS -> guardar registos em ficheiros, numa pasta
    - comunicação com PD [**Rodrigo**]
    - comunicação com User [**Rodrigo**]
    - comunicação com FS [**Sancha**]
- User 
    - comunicação com FS [**Susana**]
    - comando x e remove not working (ver comandos que faltam) [**Susana**]
- FS
    - guardar em ficheiros [**Rodrigo**]
    - comunicação com AS [**Sancha**]
    - comunicação com User [**Susana**]

