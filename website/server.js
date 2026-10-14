// The Evolutions website: the pages in public/, and a small API behind them (realm status, news, account creation).
//   node server.js            reads config.json (copy config.example.json), serves on config.port
const fs = require('fs');
const net = require('net');
const path = require('path');
const express = require('express');
const mysql = require('mysql2/promise');
const { makeRegistrationData, upperLatin } = require('./lib/srp6');

const configPath = path.join(__dirname, process.env.EVO_SITE_CONFIG || 'config.json');
if (!fs.existsSync(configPath)) {
    console.error(`Missing ${configPath}: copy config.example.json and fill it in.`);
    process.exit(1);
}
const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));

const pools = new Map();
function pool(db) {
    const key = `${db.host}:${db.port}/${db.database}`;
    if (!pools.has(key)) {
        pools.set(key, mysql.createPool({ ...db, connectionLimit: 4, charset: 'utf8mb4' }));
    }
    return pools.get(key);
}

const app = express();
app.disable('x-powered-by');
// Behind nginx the client's address is in X-Forwarded-For; the sign-up limit counts per address
if (config.trustProxy) app.set('trust proxy', 'loopback');
app.use(express.json({ limit: '10kb' }));
app.use((req, res, next) => {
    res.set('X-Content-Type-Options', 'nosniff');
    res.set('Referrer-Policy', 'strict-origin-when-cross-origin');
    res.set('X-Frame-Options', 'DENY');
    next();
});

// Realm status ---------------------------------------------------------------------------------------------------

function portOpen(host, port, timeoutMs = 2500) {
    return new Promise((resolve) => {
        const started = Date.now();
        const socket = net.connect({ host, port });
        const done = (open) => {
            socket.destroy();
            resolve(open ? Date.now() - started : null);
        };
        socket.setTimeout(timeoutMs, () => done(false));
        socket.once('connect', () => done(true));
        socket.once('error', () => done(false));
    });
}

// Bots log in from the server itself; a player from their own address
async function playersOnline(realm) {
    if (!realm.characters) return null;
    const auth = mysql.escapeId(realm.authDatabase || 'acore_auth');
    const [rows] = await pool(realm.characters).query(
        `SELECT COUNT(*) AS players FROM characters c JOIN ${auth}.account a ON a.id = c.account
         WHERE c.online = 1 AND a.last_ip NOT IN ('127.0.0.1', '')`);
    return Number(rows[0].players);
}

let statusCache = { at: 0, body: null };
async function realmStatus() {
    if (Date.now() - statusCache.at < 20000 && statusCache.body) return statusCache.body;
    const realms = await Promise.all((config.realms || []).map(async (realm) => {
        const latency = await portOpen(realm.address, realm.authPort || 3724);
        let players = null;
        if (latency !== null) {
            try {
                players = await playersOnline(realm);
            } catch (error) {
                console.warn(`Players online (${realm.id}): ${error.message}`);
            }
        }
        return { id: realm.id, name: realm.name, description: realm.description || '', online: latency !== null, players };
    }));
    statusCache = { at: Date.now(), body: { realms } };
    return statusCache.body;
}

app.get('/api/status', async (req, res) => {
    res.set('Cache-Control', 'no-store');
    res.json(await realmStatus());
});

// News: the launcher's news.json, so a post is written once for both ---------------------------------------------

const newsPath = path.resolve(__dirname, config.newsFile || '../clientPatcher/news.json');
const slug = (text) => text.normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase()
    .replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');

let newsCache = { mtime: 0, items: [] };
function news() {
    const mtime = fs.statSync(newsPath).mtimeMs;
    if (mtime !== newsCache.mtime) {
        const items = JSON.parse(fs.readFileSync(newsPath, 'utf8'));
        newsCache = {
            mtime,
            items: items.map((item) => ({ ...item, Slug: `${item.Date}-${slug(item.Title)}` })),
        };
    }
    return newsCache.items;
}

app.get('/api/news', (req, res) => {
    const limit = Math.min(Number(req.query.limit) || 100, 100);
    res.set('Cache-Control', 'public, max-age=60');
    res.json({ news: news().slice(0, limit) });
});

app.get('/api/config', (req, res) => {
    res.json({
        launcherUrl: config.launcherUrl,
        turnstileSiteKey: config.turnstile?.siteKey || '',
        discordUrl: config.discordUrl || '',
    });
});

// Account creation -----------------------------------------------------------------------------------------------

// The client's own limits: a 16 character password, plain Latin names
const USERNAME = /^[A-Za-z0-9]{3,16}$/;
const PASSWORD = /^[\x21-\x7E]{6,16}$/;
const EMAIL = /^[^\s@]{1,64}@[^\s@]{1,190}\.[^\s@]{2,}$/;

const signups = new Map();
function allowSignup(ip) {
    const hour = Date.now() - 3600000;
    const recent = (signups.get(ip) || []).filter((at) => at > hour);
    signups.set(ip, recent);
    return recent.length < (config.signupsPerHour || 3);
}

async function verifyTurnstile(token, ip) {
    const secret = config.turnstile?.secretKey;
    if (!secret) return true;
    if (!token) return false;
    const response = await fetch('https://challenges.cloudflare.com/turnstile/v0/siteverify', {
        method: 'POST',
        body: new URLSearchParams({ secret, response: token, remoteip: ip }),
    });
    return (await response.json()).success === true;
}

app.post('/api/register', async (req, res) => {
    const { username = '', email = '', password = '', confirm = '', website = '', token = '' } = req.body || {};
    const fail = (status, field, message) => res.status(status).json({ ok: false, field, message });

    // A field no person sees: only a bot fills it in. It is told everything went well.
    if (website) return res.json({ ok: true, username: upperLatin(String(username)) });

    if (!USERNAME.test(username)) {
        return fail(400, 'username', 'De 3 à 16 caractères : lettres et chiffres uniquement.');
    }
    if (!EMAIL.test(email) || email.length > 255) return fail(400, 'email', 'Adresse e-mail invalide.');
    if (!PASSWORD.test(password)) {
        return fail(400, 'password', 'De 6 à 16 caractères, sans espace ni accent.');
    }
    if (password !== confirm) return fail(400, 'confirm', 'Les deux mots de passe ne correspondent pas.');
    if (upperLatin(password) === upperLatin(username)) {
        return fail(400, 'password', 'Le mot de passe doit être différent du nom de compte.');
    }

    if (!allowSignup(req.ip)) {
        return fail(429, null, 'Trop de comptes créés depuis votre connexion. Réessayez dans une heure.');
    }
    try {
        if (!(await verifyTurnstile(token, req.ip))) {
            return fail(400, null, 'La vérification anti-robot a échoué. Réessayez.');
        }
    } catch (error) {
        console.error(`Turnstile: ${error.message}`);
        return fail(503, null, 'Vérification anti-robot indisponible. Réessayez dans un instant.');
    }

    const name = upperLatin(username);
    try {
        // One account, the same on every realm's login database (live, and the PTR when it is listed)
        for (const db of config.accounts) {
            const [rows] = await pool(db).query('SELECT 1 FROM account WHERE username = ?', [name]);
            if (rows.length) return fail(409, 'username', 'Ce nom de compte est déjà pris.');
        }
        const { salt, verifier } = makeRegistrationData(name, password);
        for (const db of config.accounts) {
            await pool(db).query(
                'INSERT INTO account (username, salt, verifier, email, reg_mail, expansion, joindate) ' +
                'VALUES (?, ?, ?, ?, ?, 2, NOW())',
                [name, salt, verifier, email, email]);
        }
    } catch (error) {
        if (error.code === 'ER_DUP_ENTRY') return fail(409, 'username', 'Ce nom de compte est déjà pris.');
        console.error(`Account creation: ${error.message}`);
        return fail(503, null, 'Le serveur de comptes ne répond pas. Réessayez dans un instant.');
    }
    signups.get(req.ip).push(Date.now());
    console.log(`Account created: ${name} (${req.ip})`);
    res.json({ ok: true, username: name });
});

// Pages ------------------------------------------------------------------------------------------------------------

// Each page is pages/<name>.html with the shared header and footer put in (<!-- header --> / <!-- footer -->)
const pagesRoot = path.join(__dirname, 'pages');
const pageCache = new Map();
function page(name) {
    const file = path.join(pagesRoot, `${name}.html`);
    // The page or a shared part edited: assembled again
    const mtime = Math.max(...[file, ...['header', 'footer'].map((part) => path.join(pagesRoot, 'partials', `${part}.html`))]
        .map((source) => fs.statSync(source).mtimeMs));
    const cached = pageCache.get(name);
    if (cached && cached.mtime === mtime) return cached.html;
    const partial = (part) => fs.readFileSync(path.join(pagesRoot, 'partials', `${part}.html`), 'utf8');
    const html = fs.readFileSync(file, 'utf8')
        .replace('<!-- header -->', partial('header'))
        .replace('<!-- footer -->', partial('footer'));
    pageCache.set(name, { mtime, html });
    return html;
}
const sendPage = (name, status = 200) => (req, res) => res.status(status).type('html').send(page(name));

app.get('/', sendPage('index'));
app.get('/inscription', sendPage('inscription'));
app.get('/telecharger', sendPage('telecharger'));
app.get('/nouveautes', sendPage('nouveautes'));
// An article has its own address, shown by the news page
app.get('/nouveautes/:slug', sendPage('nouveautes'));

app.use(express.static(path.join(__dirname, 'public'), {
    setHeaders(res, file) {
        if (/\.(jpg|png|webp|ttf|woff2?)$/.test(file)) res.set('Cache-Control', 'public, max-age=604800');
    },
}));
app.use(sendPage('404', 404));

const port = config.port || 8080;
app.listen(port, config.host || '127.0.0.1', () => {
    console.log(`Evolutions website on http://${config.host || '127.0.0.1'}:${port}`);
});
