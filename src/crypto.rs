//! Crypto + encoding helpers. Wire-compatible with the C++ `general.hpp`.
//!
//! AES-256-CBC with a random 16-byte IV prepended to the ciphertext, PKCS7
//! padding (OpenSSL default), then standard base64. The agent hex-encodes the
//! raw client bytes *before* encrypting; the server reverses with `hex_to_bytes`.

use aes::cipher::{block_padding::Pkcs7, BlockDecryptMut, BlockEncryptMut, KeyIvInit};
use base64::Engine;

type Aes256CbcEnc = cbc::Encryptor<aes::Aes256>;
type Aes256CbcDec = cbc::Decryptor<aes::Aes256>;

const IV_LEN: usize = 16;

const B64: base64::engine::general_purpose::GeneralPurpose =
    base64::engine::general_purpose::STANDARD;
const B64_NO_PAD: base64::engine::general_purpose::GeneralPurpose =
    base64::engine::general_purpose::STANDARD_NO_PAD;

/// Standard base64 with `=` padding (matches C++ `encode64`).
pub fn encode64(input: &[u8]) -> String {
    B64.encode(input)
}

/// Base64 decode. Mirrors C++ `decode64`: trailing `=` are stripped first, so
/// both padded and unpadded inputs decode.
pub fn decode64(input: &str) -> anyhow::Result<Vec<u8>> {
    let trimmed = input.trim_end_matches('=');
    B64_NO_PAD
        .decode(trimmed.as_bytes())
        .map_err(|e| anyhow::anyhow!("Invalid Base64 encoding: {e}"))
}

/// Lowercase hex of raw bytes (matches C++ `hexArrToStr` / `hexStreambufToStr`).
pub fn bytes_to_hex(data: &[u8]) -> String {
    hex::encode(data)
}

/// Hex string -> bytes (matches C++ `hexToASCII`). Invalid/odd input yields the
/// best-effort prefix, same as the C++ pairwise loop tolerating junk as 0.
pub fn hex_to_bytes(hex_str: &str) -> Vec<u8> {
    let bytes = hex_str.as_bytes();
    let mut out = Vec::with_capacity(bytes.len() / 2);
    let mut i = 0;
    while i + 1 < bytes.len() {
        out.push(16 * char_to_hex(bytes[i]) + char_to_hex(bytes[i + 1]));
        i += 2;
    }
    out
}

fn char_to_hex(c: u8) -> u8 {
    match c {
        b'0'..=b'9' => c - b'0',
        b'A'..=b'F' => c - b'A' + 10,
        b'a'..=b'f' => c - b'a' + 10,
        _ => 0,
    }
}

/// AES-256-CBC encrypt. Returns `iv || ciphertext`. Key must be 32 bytes.
pub fn aes256_encrypt(plaintext: &[u8], key: &[u8]) -> anyhow::Result<Vec<u8>> {
    let key = key32(key)?;
    let mut iv = [0u8; IV_LEN];
    rand::Rng::fill(&mut rand::thread_rng(), &mut iv);

    let ct = Aes256CbcEnc::new(&key.into(), &iv.into())
        .encrypt_padded_vec_mut::<Pkcs7>(plaintext);

    let mut out = Vec::with_capacity(IV_LEN + ct.len());
    out.extend_from_slice(&iv);
    out.extend_from_slice(&ct);
    Ok(out)
}

/// AES-256-CBC decrypt of `iv || ciphertext`. Key must be 32 bytes.
pub fn aes256_decrypt(ciphertext_with_iv: &[u8], key: &[u8]) -> anyhow::Result<Vec<u8>> {
    let key = key32(key)?;
    if ciphertext_with_iv.len() < IV_LEN {
        anyhow::bail!("ciphertext too short");
    }
    let (iv, ct) = ciphertext_with_iv.split_at(IV_LEN);
    let iv: [u8; IV_LEN] = iv.try_into().unwrap();

    Aes256CbcDec::new(&key.into(), &iv.into())
        .decrypt_padded_vec_mut::<Pkcs7>(ct)
        .map_err(|e| anyhow::anyhow!("Decryption failed: {e}"))
}

fn key32(key: &[u8]) -> anyhow::Result<[u8; 32]> {
    key.try_into()
        .map_err(|_| anyhow::anyhow!("AES key must be 32 bytes, got {}", key.len()))
}

#[cfg(test)]
mod tests {
    use super::*;

    const KEY: &[u8] = b"af445adb-2434-4975-9445-2c1b2231"; // 32 bytes, like config

    #[test]
    fn aes_round_trip() {
        let pt = b"the quick brown fox jumps over the lazy dog";
        let ct = aes256_encrypt(pt, KEY).unwrap();
        assert_eq!(ct.len() % 16, 0);
        assert!(ct.len() >= IV_LEN + 16);
        let dec = aes256_decrypt(&ct, KEY).unwrap();
        assert_eq!(dec, pt);
    }

    #[test]
    fn aes_iv_is_random() {
        let pt = b"abc";
        let a = aes256_encrypt(pt, KEY).unwrap();
        let b = aes256_encrypt(pt, KEY).unwrap();
        assert_ne!(a, b, "IV should randomize ciphertext");
        assert_eq!(aes256_decrypt(&a, KEY).unwrap(), pt);
        assert_eq!(aes256_decrypt(&b, KEY).unwrap(), pt);
    }

    #[test]
    fn base64_round_trip_and_padding() {
        let data = b"\x00\x01\x02hello\xff";
        let enc = encode64(data);
        assert!(enc.ends_with('=') || enc.len() % 4 == 0);
        assert_eq!(decode64(&enc).unwrap(), data);
        // unpadded also decodes (C++ trims '=')
        assert_eq!(decode64(enc.trim_end_matches('=')).unwrap(), data);
    }

    #[test]
    fn hex_round_trip() {
        let raw = b"\x00\xde\xad\xbeGET / HTTP/1.1\r\n";
        let h = bytes_to_hex(raw);
        assert_eq!(h, "00deadbe474554202f20485454502f312e310d0a");
        assert_eq!(hex_to_bytes(&h), raw);
    }

    #[test]
    fn full_inner_frame_round_trip() {
        // Mirrors the agent->server->agent path for the inner request.
        let client_bytes = b"CONNECT example.com:443 HTTP/1.1\r\nHost: example.com:443\r\n\r\n";
        // agent: hex -> aes -> base64
        let hexed = bytes_to_hex(client_bytes);
        let enc = aes256_encrypt(hexed.as_bytes(), KEY).unwrap();
        let wire = encode64(&enc);
        // server: base64 -> aes -> hex_to_bytes
        let dec = aes256_decrypt(&decode64(&wire).unwrap(), KEY).unwrap();
        let recovered = hex_to_bytes(std::str::from_utf8(&dec).unwrap());
        assert_eq!(recovered, client_bytes);
    }

    #[test]
    fn rejects_wrong_key_len() {
        assert!(aes256_encrypt(b"x", b"short").is_err());
    }
}
