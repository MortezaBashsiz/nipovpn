#pragma once

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <yaml-cpp/yaml.h>

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

static constexpr std::size_t BufferSize = 65536;
// Minimum token length; the AES key is derived from the whole token with SHA-256.
static constexpr std::size_t MinTokenLength = 32;
// Token shipped in the sample config; anyone can read it, so warn when it is used.
static constexpr const char *SampleToken = "af445adb-2434-4975-9445-2c1b2231";

class Uncopyable {
public:
    Uncopyable() = default;

private:
    Uncopyable(const Uncopyable &) = delete;
    Uncopyable &operator=(const Uncopyable &) = delete;
};

inline void OUT(const auto &message) {
    std::cout << "OUT OUT OUT OUT OUT OUT OUT OUT : " << message
              << std::endl;
}

struct BoolStr {
    bool ok;
    std::string message;
};

inline bool fileExists(const std::string &name) {
    std::ifstream file(name.c_str());
    return file.good();
}

inline std::string streambufToString(const boost::asio::streambuf &buff) {
    return {boost::asio::buffers_begin(buff.data()),
            boost::asio::buffers_begin(buff.data()) + buff.size()};
}

inline void copyStringToStreambuf(const std::string &inputStr,
                                  boost::asio::streambuf &buff) {
    buff.consume(buff.size());
    std::ostream os(&buff);
    os << inputStr;
}

inline void moveStreambuf(boost::asio::streambuf &source,
                          boost::asio::streambuf &target) {
    auto bytes_copied =
            boost::asio::buffer_copy(target.prepare(source.size()), source.data());
    target.commit(bytes_copied);
    source.consume(source.size());
}

inline void copyStreambuf(boost::asio::streambuf &source,
                          boost::asio::streambuf &target) {
    auto bytes_copied =
            boost::asio::buffer_copy(target.prepare(source.size()), source.data());
    target.commit(bytes_copied);
}

inline std::string hexArrToStr(const unsigned char *data, std::size_t size) {
    std::stringstream tempStr;
    tempStr << std::hex << std::setfill('0');

    for (std::size_t i = 0; i < size; ++i) {
        tempStr << std::setw(2) << static_cast<unsigned>(data[i]);
    }

    return tempStr.str();
}

inline unsigned int hexToInt(const std::string &hexString) {
    unsigned int result{};
    std::stringstream hexStr(hexString);
    hexStr >> std::hex >> result;
    return result;
}

inline std::string hexToASCII(const std::string &hex) {
    std::string ascii;

    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string part = hex.substr(i, 2);
        char ch = static_cast<char>(stoul(part, nullptr, 16));
        ascii += ch;
    }

    return ascii;
}

inline unsigned char charToHex(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    return 0;
}

inline std::vector<unsigned char> strToHexArr(const std::string &hexStr) {
    std::vector<unsigned char> result(hexStr.size() / 2);

    for (std::size_t i = 0; i < hexStr.size() / 2; ++i) {
        result[i] = 16 * charToHex(hexStr[2 * i]) + charToHex(hexStr[2 * i + 1]);
    }

    return result;
}

inline std::string hexStreambufToStr(const boost::asio::streambuf &buff) {
    return hexArrToStr(
            reinterpret_cast<const unsigned char *>(streambufToString(buff).c_str()),
            buff.size());
}

inline std::vector<std::string> splitString(const std::string &str,
                                            const std::string &delimiter) {
    std::vector<std::string> result;
    std::string tempStr = str;

    while (!tempStr.empty()) {
        size_t index = tempStr.find(delimiter);

        if (index != std::string::npos) {
            result.push_back(tempStr.substr(0, index));
            tempStr = tempStr.substr(index + delimiter.size());

            if (tempStr.empty()) {
                result.push_back(tempStr);
            }
        } else {
            result.push_back(tempStr);
            tempStr.clear();
        }
    }

    return result;
}

inline std::string decode64(const std::string &inputStr) {
    using namespace boost::archive::iterators;

    std::string temp = inputStr;
    boost::algorithm::trim_right_if(temp, boost::is_any_of("="));

    try {
        using binaryFromBase64 = binary_from_base64<std::string::const_iterator>;
        using transformWidth = transform_width<binaryFromBase64, 8, 6>;

        return {transformWidth(temp.begin()), transformWidth(temp.end())};
    } catch (const std::exception &) {
        throw std::runtime_error("Invalid Base64 encoding");
    }
}

inline std::string encode64(const std::string &inputStr) {
    using namespace boost::archive::iterators;

    using transformWidth = transform_width<std::string::const_iterator, 6, 8>;
    using base64Enc = base64_from_binary<transformWidth>;

    std::string encoded(base64Enc(inputStr.begin()), base64Enc(inputStr.end()));

    std::size_t numPadChars = (3 - inputStr.size() % 3) % 3;
    encoded.append(numPadChars, '=');

    return encoded;
}

// Payload encryption is AES-256-GCM. The key is SHA-256(token), and the wire
// format is nonce (12 bytes) || ciphertext || tag (16 bytes). The tag makes any
// tampering with the payload fail decryption.
static constexpr std::size_t AesGcmNonceLength = 12;
static constexpr std::size_t AesGcmTagLength = 16;

inline std::string deriveAesKey(const std::string &token) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    EVP_Digest(token.data(), token.size(), digest, &digestLen, EVP_sha256(), NULL);
    return std::string(reinterpret_cast<char *>(digest), digestLen);
}

inline BoolStr aes256Encrypt(const std::string &plaintext,
                             const std::string &token) {
    BoolStr result{false, "Encryption failed"};

    const std::string key = deriveAesKey(token);

    unsigned char nonce[AesGcmNonceLength];
    if (!RAND_bytes(nonce, sizeof(nonce))) {
        result.message = "Nonce generation failed";
        return result;
    }

    std::vector<unsigned char> ciphertext(plaintext.size());

    using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) {
        result.message = "Context initialization failed";
        return result;
    }

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, AesGcmNonceLength, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx.get(), NULL, NULL,
                           reinterpret_cast<const unsigned char *>(key.data()),
                           nonce) != 1) {
        result.message = "Encryption initialization failed";
        return result;
    }

    int len = 0;
    if (EVP_EncryptUpdate(
                ctx.get(), ciphertext.data(), &len,
                reinterpret_cast<const unsigned char *>(plaintext.data()),
                plaintext.size()) != 1) {
        result.message = "Encryption update failed";
        return result;
    }
    int ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &len) != 1) {
        result.message = "Encryption finalization failed";
        return result;
    }
    ciphertext_len += len;

    unsigned char tag[AesGcmTagLength];
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, AesGcmTagLength, tag) != 1) {
        result.message = "Tag generation failed";
        return result;
    }

    std::string final_result(reinterpret_cast<char *>(nonce), sizeof(nonce));
    final_result.append(reinterpret_cast<char *>(ciphertext.data()), ciphertext_len);
    final_result.append(reinterpret_cast<char *>(tag), sizeof(tag));

    result.ok = true;
    result.message = final_result;
    return result;
}

inline BoolStr aes256Decrypt(const std::string &sealed,
                             const std::string &token) {
    BoolStr result{false, "Decryption failed"};

    if (sealed.size() < AesGcmNonceLength + AesGcmTagLength) {
        result.message = "Ciphertext too short";
        return result;
    }

    const std::string key = deriveAesKey(token);

    const auto *nonce = reinterpret_cast<const unsigned char *>(sealed.data());
    const auto *ciphertext = nonce + AesGcmNonceLength;
    const std::size_t ciphertextLen = sealed.size() - AesGcmNonceLength - AesGcmTagLength;
    std::string tag = sealed.substr(sealed.size() - AesGcmTagLength);

    std::vector<unsigned char> plaintext(ciphertextLen);

    using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) {
        result.message = "Context initialization failed";
        return result;
    }

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, AesGcmNonceLength, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx.get(), NULL, NULL,
                           reinterpret_cast<const unsigned char *>(key.data()),
                           nonce) != 1) {
        result.message = "Decryption initialization failed";
        return result;
    }

    int len = 0;
    if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len, ciphertext,
                          ciphertextLen) != 1) {
        result.message = "Decryption update failed";
        return result;
    }
    int plaintext_len = len;

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, AesGcmTagLength,
                            tag.data()) != 1) {
        result.message = "Setting tag failed";
        return result;
    }

    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &len) != 1) {
        result.message = "Authentication failed (wrong token or tampered payload)";
        return result;
    }
    plaintext_len += len;

    result.ok = true;
    result.message =
            std::string(reinterpret_cast<char *>(plaintext.data()), plaintext_len);

    return result;
}

// Agent -> server request payload: "<action>\n<session>\n<data>". It is carried
// inside the (encrypted) body so no custom headers are needed on the wire.
inline std::string makeEnvelope(const std::string &action,
                                const std::string &session,
                                const std::string &data) {
    return action + "\n" + session + "\n" + data;
}

struct Envelope {
    std::string action;
    std::string session;
    std::string data;
};

inline bool parseEnvelope(const std::string &raw, Envelope &out) {
    const auto actionEnd = raw.find('\n');
    if (actionEnd == std::string::npos) return false;

    const auto sessionEnd = raw.find('\n', actionEnd + 1);
    if (sessionEnd == std::string::npos) return false;

    out.action = raw.substr(0, actionEnd);
    out.session = raw.substr(actionEnd + 1, sessionEnd - actionEnd - 1);
    out.data = raw.substr(sessionEnd + 1);
    return true;
}

inline BoolStr validateConfig(int argc, const char *argv[]) {
    BoolStr result{false, "Validation failed"};

    if (argc != 3) {
        result.message =
                "Requires 2 arguments: mode [server|agent] and config file "
                "path\nExample: nipovpn server config.yaml\n";
        return result;
    }

    if (std::string(argv[1]) != "server" && std::string(argv[1]) != "agent") {
        result.message = "First argument must be one of [server|agent]\n";
        return result;
    }

    if (!fileExists(argv[2])) {
        result.message =
                std::string("Config file ") + argv[2] + " does not exist\n";
        return result;
    }

    YAML::Node configYaml;
    try {
        configYaml = YAML::LoadFile(argv[2]);
    } catch (const std::exception &e) {
        result.message =
                std::string("Error parsing config file: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["general"]["token"].as<std::string>();
        configYaml["general"]["protocol"].as<std::string>();
        configYaml["general"]["bufferSize"].as<unsigned int>();
        configYaml["general"]["fakeUrls"].as<std::vector<std::string>>();
        configYaml["general"]["methods"].as<std::vector<std::string>>();
        configYaml["general"]["endPoints"].as<std::vector<std::string>>();
        configYaml["general"]["timeout"].as<unsigned short>();
        configYaml["general"]["pullTimeout"].as<unsigned short>();
        configYaml["general"]["tunnelEnable"].as<bool>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'general' block: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["log"]["logFile"].as<std::string>();
        configYaml["log"]["logLevel"].as<std::string>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'log' block: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["server"]["listenIp"].as<std::string>();
        configYaml["server"]["listenPort"].as<unsigned short>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'server' block: ") + e.what() + "\n";
        return result;
    }

    try {
        configYaml["agent"]["listenIp"].as<std::string>();
        configYaml["agent"]["listenPort"].as<unsigned short>();
        configYaml["agent"]["serverIp"].as<std::string>();
        configYaml["agent"]["serverPort"].as<unsigned short>();
        configYaml["agent"]["httpVersion"].as<std::string>();
        configYaml["agent"]["userAgent"].as<std::string>();
    } catch (const std::exception &e) {
        result.message = std::string("Error in 'agent' block: ") + e.what() + "\n";
        return result;
    }

    if (configYaml["general"]["protocol"].as<std::string>() != "http" && configYaml["general"]["protocol"].as<std::string>() != "socks5") {
        result.message = "protocol must be one of [http|socks5]\n";
        return result;
    }

    if (configYaml["general"]["bufferSize"].as<unsigned int>() < 1 || configYaml["general"]["bufferSize"].as<unsigned int>() > 65536) {
        result.message = "bufferSize must be between 1 and 65536\n";
        return result;
    }

    if (configYaml["general"]["aesEnable"].as<bool>(true) &&
        configYaml["general"]["token"].as<std::string>().size() < MinTokenLength) {
        result.message = "token must be at least 32 bytes when aesEnable is true\n";
        return result;
    }

    result.ok = true;
    result.message = "OK";
    return result;
}