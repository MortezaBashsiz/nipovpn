#pragma once
#ifndef GENERAL_HPP
#define GENERAL_HPP

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
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief Base class to prevent copying of derived classes.
 *
 * This class is used to make derived classes uncopyable by deleting the
 * copy constructor and copy assignment operator.
 */
class Uncopyable {
public:
    Uncopyable() = default;

private:
    Uncopyable(const Uncopyable &) = delete;
    Uncopyable &operator=(const Uncopyable &) = delete;
};

/*
 * FUCK function prints it on screen
 */
inline void FUCK(const auto &message) {
    std::cout << "FUCK FUCK FUCK FUCK FUCK FUCK FUCK FUCK : " << message
              << std::endl;
}

/**
 * @brief Structure to return a boolean status and a message.
 *
 * This is used for returning status and messages from functions, particularly
 * those dealing with encryption or configuration validation.
 */
struct BoolStr {
    bool ok;            ///< Indicates success or failure.
    std::string message;///< Message describing the result or error.
};

/**
 * @brief Checks if a file exists.
 *
 * @param name The name of the file to check.
 * @return true if the file exists, false otherwise.
 */
inline bool fileExists(const std::string &name) {
    namespace fs = std::filesystem;
    return fs::exists(fs::path{name});
}

/**
 * @brief Converts a `boost::asio::streambuf` to a `std::string`.
 *
 * @param buff The streambuf to convert.
 * @return The resulting string.
 */
inline std::string streambufToString(const boost::asio::streambuf &buff) {
    return {boost::asio::buffers_begin(buff.data()),
            boost::asio::buffers_begin(buff.data()) + buff.size()};
}

/**
 * @brief Copies a string to a `boost::asio::streambuf`.
 *
 * @param inputStr The string to copy.
 * @param buff The target streambuf.
 */
inline void copyStringToStreambuf(const std::string &inputStr,
                                  boost::asio::streambuf &buff) {
    buff.consume(buff.size());
    std::ostream os(&buff);
    os << inputStr;
}

/**
 * @brief Moves data from one `boost::asio::streambuf` to another.
 *
 * @param source The source streambuf.
 * @param target The target streambuf.
 */
inline void moveStreambuf(boost::asio::streambuf &source,
                          boost::asio::streambuf &target) {
    auto bytes_copied =
            boost::asio::buffer_copy(target.prepare(source.size()), source.data());
    target.commit(bytes_copied);
    source.consume(source.size());
}

/**
 * @brief Copies data from one `boost::asio::streambuf` to another.
 *
 * @param source The source streambuf.
 * @param target The target streambuf.
 */
inline void copyStreambuf(boost::asio::streambuf &source,
                          boost::asio::streambuf &target) {
    auto bytes_copied =
            boost::asio::buffer_copy(target.prepare(source.size()), source.data());
    target.commit(bytes_copied);
}

/**
 * @brief Converts a hex array to a string.
 *
 * @param data Pointer to the data array.
 * @param size The size of the data array.
 * @return The resulting string.
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
 * @brief Converts a hex string to an integer.
 *
 * @param hexString The hex string.
 * @return The resulting integer.
 */
inline unsigned short hexToInt(const std::string &hexString) {
    unsigned short result;
    std::stringstream hexStr(hexString);
    hexStr >> std::hex >> result;
    return result;
}

/**
 * @brief Converts a hex string to ASCII.
 *
 * @param hex The hex string.
 * @return The resulting ASCII string.
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
 * @brief Converts a character to its hex value.
 *
 * @param c The character.
 * @return The hex value.
 */
inline unsigned char charToHex(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    return 0;
}

/**
 * @brief Converts a hex string to a hex array.
 *
 * @param hexStr The hex string.
 * @return The resulting hex array.
 */
inline std::vector<unsigned char> strToHexArr(const std::string &hexStr) {
    std::vector<unsigned char> result(hexStr.size() / 2);
    for (std::size_t i = 0; i < hexStr.size() / 2; ++i)
        result[i] = 16 * charToHex(hexStr[2 * i]) + charToHex(hexStr[2 * i + 1]);
    return result;
}

/**
 * @brief Converts a hex `boost::asio::streambuf` to a string.
 *
 * @param buff The streambuf.
 * @return The resulting string.
 */
inline std::string hexStreambufToStr(const boost::asio::streambuf &buff) {
    return hexArrToStr(
            reinterpret_cast<const unsigned char *>(streambufToString(buff).c_str()),
            buff.size());
}

/**
 * @brief Splits a string by a specified token.
 *
 * @param str The string to split.
 * @param token The token to split by.
 * @return A vector of strings resulting from the split.
 */
inline std::vector<std::string> splitString(const std::string &str,
                                            const std::string &token) {
    std::vector<std::string> result;
    std::string tempStr = str;
    while (!tempStr.empty()) {
        size_t index = tempStr.find(token);
        if (index != std::string::npos) {
            result.push_back(tempStr.substr(0, index));
            tempStr = tempStr.substr(index + token.size());
            if (tempStr.empty()) result.push_back(tempStr);
        } else {
            result.push_back(tempStr);
            tempStr.clear();
        }
    }
    return result;
}

/**
 * @brief Decodes a Base64 encoded string.
 *
 * @param inputStr The Base64 encoded string.
 * @return The decoded string.
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
 * @brief Encodes a string to Base64.
 *
 * @param inputStr The string to encode.
 * @return The Base64 encoded string.
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
 * @brief Encrypts a plaintext string using AES-256-CBC.
 *
 * @param plaintext The plaintext string.
 * @param key The encryption key.
 * @return BoolStr containing success status and the encrypted message.
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
 * @brief Decrypts an AES-256-CBC encrypted string.
 *
 * @param ciphertext_with_iv The encrypted string with IV.
 * @param key The decryption key.
 * @return BoolStr containing success status and the decrypted message.
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
 * @brief Validates the command-line arguments and configuration file.
 *
 * @param argc The number of command-line arguments.
 * @param argv The command-line arguments.
 * @return BoolStr containing success status and validation message.
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
        configYaml["agent"]["token"].as<std::string>();
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

#endif// GENERAL_HPP
