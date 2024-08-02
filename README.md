# NipoVPN
A powerfull proxy to hide your http packets inside a fake http request.



```mermaid
sequenceDiagram
Note left of Client: Set http(s)_proxy
Client ->> nipoAgent: GET https://www.google.com/ 
loop
    nipoAgent-->>nipoAgent: Encrypt, Encode
    nipoAgent-->>nipoAgent: Prepare fake http request
end
nipoAgent ->> nipoServer: Fake http request
loop
    nipoServer-->>nipoServer: Decrypt, Decode
end
nipoServer ->> Origin: Connection google.com:443(TCP handshake)
Origin ->> nipoServer: Connection response(TCP handshake)
Note right of nipoServer: Connection established to google.com
loop
    nipoServer-->>nipoServer: Encrypt, Encode
    nipoServer-->>nipoServer: Prepare fake http response
end
nipoServer ->> nipoAgent: Fake http response
nipoAgent ->> Client: 200 Connection established
Client ->> nipoAgent: Original request
nipoAgent ->> nipoServer: Fake http request
nipoServer ->> Origin: Original request
Origin ->> nipoServer: Original response
loop
    nipoServer-->>nipoServer: Encrypt, Encode
    nipoServer-->>nipoServer: Prepare fake http response
end
nipoServer ->> nipoAgent: Fake http response
loop
    nipoServer-->>nipoServer: Decrypt, Decode
end
nipoAgent ->> Client: Original response


```
