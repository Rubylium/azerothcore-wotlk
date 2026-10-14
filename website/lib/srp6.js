// The account's salt and verifier, exactly as the auth server makes them (src/common/Cryptography/Authentication/
// SRP6.cpp, AccountMgr::CreateAccount): v = g ^ H(s || H(USER ':' PASS)) mod N, numbers little-endian, the name and
// password upper-cased (Latin letters only, as Utf8ToUpperOnlyLatin).
const crypto = require('crypto');

const N = BigInt('0x894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7');
const g = 7n;

function sha1(...parts) {
    const hash = crypto.createHash('sha1');
    for (const part of parts) hash.update(part);
    return hash.digest();
}

function fromLittleEndian(buffer) {
    return BigInt('0x' + (Buffer.from(buffer).reverse().toString('hex') || '0'));
}

function toLittleEndian(value, length) {
    const hex = value.toString(16).padStart(length * 2, '0');
    return Buffer.from(hex, 'hex').reverse();
}

function modPow(base, exponent, modulus) {
    let result = 1n;
    base %= modulus;
    while (exponent > 0n) {
        if (exponent & 1n) result = (result * base) % modulus;
        exponent >>= 1n;
        base = (base * base) % modulus;
    }
    return result;
}

const upperLatin = (text) => text.replace(/[a-z]/g, (letter) => letter.toUpperCase());

function calculateVerifier(username, password, salt) {
    const inner = sha1(upperLatin(username), ':', upperLatin(password));
    const x = fromLittleEndian(sha1(salt, inner));
    return toLittleEndian(modPow(g, x, N), 32);
}

function makeRegistrationData(username, password) {
    const salt = crypto.randomBytes(32);
    return { salt, verifier: calculateVerifier(username, password, salt) };
}

module.exports = { calculateVerifier, makeRegistrationData, upperLatin };
