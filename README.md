# NipoVPN
A powerfull proxy to hide your http packets inside a fake http request.

## Flow
Here you can see the logical flow of a single request from first step to get the response
```mermaid
sequenceDiagram
Note left of Client: Set Proxy

Client ->> nipoAgent: TCP Handshake
Client ->> nipoAgent: CONNECT google.com

nipoAgent-->>nipoAgent: Encrypt, Encode, Prepare fake http request

nipoAgent ->> nipoServer: TCP Handshake

nipoAgent ->> nipoServer: Fake http request
nipoServer-->>nipoServer: Decrypt, Decode

nipoServer ->> Origin: TCP Handshake

nipoServer-->>nipoServer: Prepare 200 Cnnection established
nipoServer-->>nipoServer: Encrypt, Encode, Prepare fake http response
    
nipoServer ->> nipoAgent: Fake http response

nipoAgent-->>nipoAgent: Decrypt, Decode, Prepare original response

nipoAgent ->> Client: 200 Connection established
Client ->> nipoAgent: Original request

nipoAgent-->>nipoAgent: Encrypt, Encode, Prepare fake http request

nipoAgent ->> nipoServer: Fake http request

nipoServer-->>nipoServer: Decrypt, Decode, Prepare original request

nipoServer ->> Origin: Original request
Origin ->> nipoServer: Original response

nipoServer-->>nipoServer: Encrypt, Encode, Prepare fake http response

nipoServer ->> nipoAgent: Fake http response

nipoAgent-->>nipoAgent: Decrypt, Decode, Prepare original response

nipoAgent ->> Client: Original response