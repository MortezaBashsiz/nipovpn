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
#include <sstream>
#include <string>
#include <vector>

/**
* @brief Abstract for inheritance to make classes Uncopyable.
*/
class Uncopyable {
public:
    Uncopyable() = default;

private:
    Uncopyable(const Uncopyable &) = delete;
    Uncopyable &operator=(const Uncopyable &) = delete;
};

/**
* @brief Prints message to output.
* 
* @param message an auto refrence to message.
*/
inline void OUT(const auto &message) {
    std::cout << "OUT OUT OUT OUT OUT OUT OUT OUT : " << message
              << std::endl;
}

/**
* @brief struct for return a bool with specific message.
*/
struct BoolStr {
    bool ok;
    std::string message;
};

/**
* @brief checks if the file exists or not.
* 
* @param name which is refrenced to the string path of file.
* @return bool as result of existance.
*/
inline bool fileExists(const std::string &name) {
    std::ifstream file(name.c_str());
    return file.good();
}

/**
* @brief converts streambuffer to string.
* 
* @param buff is a refrence to streambuffer.
* @return string format of given buffer.
*/
inline std::string streambufToString(const boost::asio::streambuf &buff) {
    return {boost::asio::buffers_begin(buff.data()),
            boost::asio::buffers_begin(buff.data()) + buff.size()};
}

/**
* @brief copies a string to streambuffer.
* 
* @param inputStr a refrence to the string.
* @param buff a refrence to the streambuffer.
*/
inline void copyStringToStreambuf(const std::string &inputStr,
                                  boost::asio::streambuf &buff) {
    buff.consume(buff.size());
    std::ostream os(&buff);
    os << inputStr;
}

/**
* @brief moves a streambuffer to another streambuffer.
*   it will consume(clean) the source streambuffer.
* 
* @param source a refrence to the source streambuffer.
* @param target a refrence to the target streambuffer.
*/
inline void moveStreambuf(boost::asio::streambuf &source,
                          boost::asio::streambuf &target) {
    auto bytes_copied =
            boost::asio::buffer_copy(target.prepare(source.size()), source.data());
    target.commit(bytes_copied);
    source.consume(source.size());
}

/**
* @brief copies a streambuffer to another streambuffer.
*   it will keep the source streambuffer as is.
* 
* @param source a refrence to the source streambuffer.
* @param target a refrence to the target streambuffer.
*/
inline void copyStreambuf(boost::asio::streambuf &source,
                          boost::asio::streambuf &target) {
    auto bytes_copied =
            boost::asio::buffer_copy(target.prepare(source.size()), source.data());
    target.commit(bytes_copied);
}

/**
* @brief converts hexadecimal array to string.
* 
* @param data is a refrence to the hexadecimal array.
* @size is size of hexadecimal array.
* @return string format of given array.
*/
inline std::string hexArrToStr(const unsigned char *data, std::size_t size) {
    std::stringstream tempStr;
    tempStr << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        tempStr << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return tempStr.str();
}

/**
* @brief converts hexadecimal string to int.
* 
* @param hexString is a refrence to the hexadecimal string.
* @return the result as unsigned int.
*/
inline unsigned int hexToInt(const std::string &hexString) {
    unsigned short result;
    std::stringstream hexStr(hexString);
    hexStr >> std::hex >> result;
    return result;
}

/**
* @brief converts hexadecimal string to ASCII string format.
* 
* @param hex is a refrence to the hexadecimal string.
* @return the result as string.
*/
inline std::string hexToASCII(const std::string &hex) {
    std::string ascii;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string part = hex.substr(i, 2);
        char ch = static_cast<char>(stoul(part, nullptr, 16));
        ascii += ch;
    }
    return ascii;
}


/**
* @brief converts a char hexadecimal char.
* 
* @param c is a char.
* @return the result as hex char.
*/
inline unsigned char charToHex(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    return 0;
}

/**
* @brief converts string to hexadecimal array.
* 
* @param hexStr is a refrence to the hexadecimal string.
* @return the result as a vector of chars.
*/
inline std::vector<unsigned char> strToHexArr(const std::string &hexStr) {
    std::vector<unsigned char> result(hexStr.size() / 2);
    for (std::size_t i = 0; i < hexStr.size() / 2; ++i)
        result[i] = 16 * charToHex(hexStr[2 * i]) + charToHex(hexStr[2 * i + 1]);
    return result;
}

/**
* @brief converts streambuffer to string.
*   first we need to convert the streambuffer to hex array and then to string
* 
* @param buff is a refrence to the streambuffer.
* @return the result as a string.
*/
inline std::string hexStreambufToStr(const boost::asio::streambuf &buff) {
    return hexArrToStr(
            reinterpret_cast<const unsigned char *>(streambufToString(buff).c_str()),
            buff.size());
}

/**
* @brief splits string based on delimiter.
* 
* @param str is a refrence to the string.
* @param delimiter is a refrence to the delimiter string.
* @return the result as a vector of strings.
*/
inline std::vector<std::string> splitString(const std::string &str,
                                            const std::string &delimiter) {
    std::vector<std::string> result;
    std::string tempStr = str;
    while (!tempStr.empty()) {
        size_t index = tempStr.find(delimiter);
        if (index != std::string::npos) {
            result.push_back(tempStr.substr(0, index));
            tempStr = tempStr.substr(index + delimiter.size());
            if (tempStr.empty()) result.push_back(tempStr);
        } else {
            result.push_back(tempStr);
            tempStr.clear();
        }
    }
    return result;
}


/**
* @brief decodes based64 encoded string.
* 
* @param inputStr is a refrence to the encoded string.
* @return decoded string.
*/
inline std::string decode64(const std::string &inputStr) {
    using namespace boost::archive::iterators;

    std::string temp = inputStr;
    boost::algorithm::trim_right_if(temp, boost::is_any_of("="));

    try {
        using binaryFromBase64 = binary_from_base64<std::string::const_iterator>;
        using transformWidth = transform_width<binaryFromBase64, 8, 6>;

        return {transformWidth(temp.begin()), transformWidth(temp.end())};
    } catch (const std::exception &e) {
        throw std::runtime_error("Invalid Base64 encoding");
    }
}

/**
* @brief encodes string in base64 format.
* 
* @param inputStr is a refrence to the string.
* @return encoded string.
*/
inline std::string encode64(const std::string &inputStr) {
    using namespace boost::archive::iterators;

    using transformWidth = transform_width<std::string::const_iterator, 6, 8>;
    using base64Enc = base64_from_binary<transformWidth>;

    std::string encoded(base64Enc(inputStr.begin()), base64Enc(inputStr.end()));

    std::size_t numPadChars = (3 - inputStr.size() % 3) % 3;
    encoded.append(numPadChars, '=');

    return encoded;
}

/**
* @brief encrypt string in aes256 by given key.
* 
* @param plaintext is a refrence to the string.
* @param key is a refrence to the key string.
* @return encrypted BoolStr.
*/
inline BoolStr aes256Encrypt(const std::string &plaintext,
                             const std::string &key) {
    BoolStr result{false, "Encryption failed"};

    unsigned char iv[EVP_MAX_IV_LENGTH];
    if (!RAND_bytes(iv, sizeof(iv))) {
        result.message = "IV generation failed";
        return result;
    }

    int ciphertext_len =
            plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc());
    std::vector<unsigned char> ciphertext(ciphertext_len);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        result.message = "Context initialization failed";
        return result;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                           reinterpret_cast<const unsigned char *>(key.c_str()),
                           iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Encryption initialization failed";
        return result;
    }

    int len;
    if (EVP_EncryptUpdate(
                ctx, ciphertext.data(), &len,
                reinterpret_cast<const unsigned char *>(plaintext.c_str()),
                plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Encryption update failed";
        return result;
    }
    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Encryption finalization failed";
        return result;
    }
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    std::string final_result(reinterpret_cast<char *>(iv), sizeof(iv));
    final_result.append(reinterpret_cast<char *>(ciphertext.data()),
                        ciphertext_len);
    result.ok = true;
    result.message = final_result;

    return result;
}

/**
* @brief decrypt string in aes256 by given key.
* 
* @param ciphertext_with_iv is a refrence to the encrypted string.
* @param key is a refrence to the key string.
* @return decrypted BoolStr.
*/
inline BoolStr aes256Decrypt(const std::string &ciphertext_with_iv,
                             const std::string &key) {
    BoolStr result{false, "Decryption failed"};

    unsigned char iv[EVP_MAX_IV_LENGTH];
    std::memcpy(iv, ciphertext_with_iv.c_str(),
                EVP_CIPHER_iv_length(EVP_aes_256_cbc()));
    std::string ciphertext =
            ciphertext_with_iv.substr(EVP_CIPHER_iv_length(EVP_aes_256_cbc()));

    int max_plaintext_len = ciphertext.size();
    std::vector<unsigned char> plaintext(max_plaintext_len);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        result.message = "Context initialization failed";
        return result;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                           reinterpret_cast<const unsigned char *>(key.c_str()),
                           iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Decryption initialization failed";
        return result;
    }

    int len;
    if (EVP_DecryptUpdate(
                ctx, plaintext.data(), &len,
                reinterpret_cast<const unsigned char *>(ciphertext.c_str()),
                ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Decryption update failed";
        return result;
    }
    int plaintext_len = len;

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        result.message = "Decryption finalization failed";
        return result;
    }
    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);

    result.ok = true;
    result.message =
            std::string(reinterpret_cast<char *>(plaintext.data()), plaintext_len);

    return result;
}

/**
* @brief validates if the config is correct or not.
*
* @return result BoolStr.
*/
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
        configYaml["general"]["fakeUrl"].as<std::string>();
        configYaml["general"]["method"].as<std::string>();
        configYaml["general"]["timeWait"].as<unsigned int>();
        configYaml["general"]["timeout"].as<unsigned short>();
        configYaml["general"]["repeatWait"].as<unsigned short>();
        configYaml["general"]["chunkHeader"].as<std::string>();
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

    result.ok = true;
    result.message = "OK";
    return result;
}
