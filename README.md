# NipoVPN
A powerfull proxy to hide your http packets inside a fake http request.



```mermaid
sequenceDiagram
Note left of Client: Set http(s)_proxy and send CONNECT
Client ->> nipoAgent: GET https://www.google.com/ 
loop
    nipoAgent-->>nipoAgent: Encrypt, Encode
end
Note right of nipoAgent: Prepare fake customizable http request, Method, Domain, URL

nipoAgent ->> nipoServer: Fake http request
loop
    nipoServer-->>nipoServer: Decrypt, Decode
end
nipoServer ->> Origin: Connection to google.com (TCP handshake)
Origin ->> nipoServer: Connect response (TCP handshake)
Note right of nipoServer: Connection established to google.com
nipoServer ->> nipoAgent: 200 Connection established
nipoAgent ->> Client: 200 Connection established
Client ->> nipoAgent: Original request
nipoAgent ->> nipoServer: Fake http request
nipoServer ->> Origin: Original request
Origin ->> nipoServer: Original response
Note left of nipoServer: Encrypt, Encode
nipoServer ->> nipoAgent: Fake http response
Note right of nipoAgent: Decrypt, Decode
nipoAgent ->> Client: Original response


```
