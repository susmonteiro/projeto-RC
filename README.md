## RC Two-Factor Authentication

### Introdução
Uso de password + envio de um código para o dispositivo pessoal

The development:

1. [Authentication Server (AS)](#as)
2. [File Server (FS)](#fs)
3. [Personal Device Application (PD)](#pd)
4. [User Application (User)](#user)

*AS* and *FS* will be running on machines with known IP addresses and ports.

The **user** (a person) has simultaneously access to *PD* and *User*

The **user**, after completing the two-factor authentication procedure, can ask to **list**, **upload**, **retrieve** or **delete** files to/from the *FS*, as well as to **remove** all its information.
For any single transaction with the *FS*, the **user** must first request a new transaction ID (*TID*) to the *AS*

Full operation:

**User** registers with the *AS* using the *PD*, using a *UID* (`5 digit number` user id) and a *pass* (`8 alphanumeric characters` password). At each registration, the *PD* sends it IP and UDP port for the AS to be able to contact back to the *PD*
> First registation defines the pair *UID/pass* at the AS database: check if the *UID* already exists (and if the given password is correct) or adds the pair to the database







