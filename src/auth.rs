use std::fs;
use std::path::Path;

use anyhow::{Context, Result};
use base64::engine::general_purpose::{STANDARD, URL_SAFE_NO_PAD};
use base64::Engine;
use ed25519_dalek::pkcs8::{DecodePrivateKey, EncodePublicKey};
use ed25519_dalek::{Signer, SigningKey, VerifyingKey};
use sha2::{Digest, Sha256};

#[derive(Clone)]
pub struct SigningIdentity {
    pub key_id: String,
    pub public_key_payload: String,
    pub signing_key: SigningKey,
}

impl SigningIdentity {
    pub fn load(private_key_path: &Path, _public_key_path: Option<&Path>) -> Result<Self> {
        let pem = fs::read_to_string(private_key_path)
            .with_context(|| format!("failed to read private key {}", private_key_path.display()))?;
        let signing_key = SigningKey::from_pkcs8_pem(&pem).context("failed to parse Ed25519 PKCS8 private key")?;
        let verifying_key = VerifyingKey::from(&signing_key);
        let public_key_der = verifying_key
            .to_public_key_der()
            .context("failed to encode public key as DER")?;
        let public_key_bytes = public_key_der.as_bytes();

        Ok(Self {
            key_id: format!("SHA256:{}", URL_SAFE_NO_PAD.encode(Sha256::digest(public_key_bytes))),
            public_key_payload: STANDARD.encode(public_key_bytes),
            signing_key,
        })
    }
}

pub fn build_canonical_request(
    method: &str,
    pathname: &str,
    timestamp: u128,
    nonce: &str,
    key_id: &str,
    body: &str,
) -> String {
    [
        method.to_uppercase(),
        pathname.to_string(),
        timestamp.to_string(),
        nonce.to_string(),
        key_id.to_string(),
        body.to_string(),
    ]
    .join("\n")
}

pub fn sign_canonical_request(identity: &SigningIdentity, canonical_request: &str) -> String {
    STANDARD.encode(identity.signing_key.sign(canonical_request.as_bytes()).to_bytes())
}
