// Logs in to an auth server as the 3.3.5a client does (SRP6 challenge and proof), to check that an account's
// password works - in particular one the website created.
//   node tools/checkLogin.js <host> <username> <password>     exit 0 when the server accepts the password
const crypto = require('crypto');
const net = require('net');

const [host, username, password] = process.argv.slice(2);
if (!password) {
    console.error('Usage: node tools/checkLogin.js <host> <username> <password>');
    process.exit(2);
}

const sha1 = (...parts) => {
    const hash = crypto.createHash('sha1');
    for (const part of parts) hash.update(part);
    return hash.digest();
};
const fromLE = (buffer) => BigInt('0x' + (Buffer.from(buffer).reverse().toString('hex') || '0'));
const toLE = (value, length) => Buffer.from(value.toString(16).padStart(length * 2, '0'), 'hex').reverse();
const modPow = (base, exponent, modulus) => {
    let result = 1n;
    base %= modulus;
    while (exponent > 0n) {
        if (exponent & 1n) result = (result * base) % modulus;
        exponent >>= 1n;
        base = (base * base) % modulus;
    }
    return result;
};
const mod = (value, modulus) => ((value % modulus) + modulus) % modulus;

// SRP6::SHA1Interleave: the session key from S
function interleave(S) {
    let p = 0;
    while (p < 32 && !S[p]) p++;
    if (p & 1) p++;
    p /= 2;
    const even = Buffer.alloc(16);
    const odd = Buffer.alloc(16);
    for (let i = 0; i < 16; i++) {
        even[i] = S[2 * i];
        odd[i] = S[2 * i + 1];
    }
    const h0 = sha1(even.subarray(p));
    const h1 = sha1(odd.subarray(p));
    const key = Buffer.alloc(40);
    for (let i = 0; i < 20; i++) {
        key[2 * i] = h0[i];
        key[2 * i + 1] = h1[i];
    }
    return key;
}

const user = username.toUpperCase();
const pass = password.toUpperCase();

const challenge = Buffer.alloc(34 + user.length);
challenge.writeUInt8(0x00, 0);               // AUTH_LOGON_CHALLENGE
challenge.writeUInt8(0x08, 1);
challenge.writeUInt16LE(30 + user.length, 2);
challenge.write('WoW\0', 4, 'latin1');
challenge.writeUInt8(3, 8);
challenge.writeUInt8(3, 9);
challenge.writeUInt8(5, 10);
challenge.writeUInt16LE(12340, 11);
challenge.write('68x\0', 13, 'latin1');      // "x86", reversed as the client sends it
challenge.write('niW\0', 17, 'latin1');
challenge.write('SUne', 21, 'latin1');
challenge.writeUInt32LE(60, 25);
challenge.writeUInt32LE(0x0100007f, 29);
challenge.writeUInt8(user.length, 33);
challenge.write(user, 34, 'latin1');

const socket = net.connect({ host, port: 3724 });
let received = Buffer.alloc(0);
let stage = 'challenge';
const finish = (code, text) => {
    console.log(text);
    socket.destroy();
    process.exit(code);
};
socket.setTimeout(8000, () => finish(1, 'No answer from the auth server.'));
socket.on('error', (error) => finish(1, `Connection failed: ${error.message}`));
socket.on('connect', () => socket.write(challenge));
socket.on('data', (chunk) => {
    received = Buffer.concat([received, chunk]);
    if (stage === 'challenge') {
        if (received.length < 3) return;
        if (received[2] !== 0) return finish(1, `Challenge refused (error ${received[2]}: unknown account?).`);
        if (received.length < 119) return;
        const B = received.subarray(3, 35);
        const g = fromLE(received.subarray(36, 37));
        const nBytes = received.subarray(38, 70);
        const N = fromLE(nBytes);
        const salt = received.subarray(70, 102);

        const a = fromLE(crypto.randomBytes(19));
        const A = toLE(modPow(g, a, N), 32);
        const u = fromLE(sha1(A, B));
        const x = fromLE(sha1(salt, sha1(user, ':', pass)));
        const S = toLE(modPow(mod(fromLE(B) - 3n * modPow(g, x, N), N), a + u * x, N), 32);
        const K = interleave(S);
        const hN = sha1(nBytes);
        const hg = sha1(toLE(g, 1));
        for (let i = 0; i < 20; i++) hN[i] ^= hg[i];
        const M1 = sha1(hN, sha1(user), salt, A, B, K);

        const proof = Buffer.alloc(75);
        proof.writeUInt8(0x01, 0);               // AUTH_LOGON_PROOF
        A.copy(proof, 1);
        M1.copy(proof, 33);
        stage = 'proof';
        received = Buffer.alloc(0);
        socket.write(proof);
    } else if (received.length >= 2) {
        if (received[1] === 0) finish(0, `Password accepted: ${user} can log in.`);
        else finish(1, `Password refused (error ${received[1]}).`);
    }
});
