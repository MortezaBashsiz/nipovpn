# NipoVPN
A powerfull proxy to hide your http packets inside a fake http request.



```mermaid
graph TD;
    Client-->Agent;
    Agent-->Server;
    Server-->Origin;
    Origin-->Server;
    Server-->Agent;
    Agent-->Client;
```
