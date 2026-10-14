# Evolutions website

The server's public site: showcase, news, realm status and account creation. Node.js (Express + mysql2), plain
HTML/CSS/JS pages in the launcher's look (palette and Cinzel from `clientPatcher/launcher/Theme.xaml`).

## Run it locally

```sh
cd website
npm install
cp config.example.json config.json   # database, realms, launcher link
node server.js                       # http://127.0.0.1:8080
```

It needs the local MySQL (`localTools/startDatabase.ps1`); the game servers can stay off.

## What is where

- `pages/*.html`: the pages; the server puts `pages/partials/header.html` and `footer.html` into each.
- `public/`: CSS, JS, fonts and pictures. `public/img` is made by `python website/tools/buildImages.py` from the art
  the launcher and the client already use (committed; run it again after changing that art).
- `server.js`: the API.
  - `GET /api/status` checks each realm's auth port, and counts online players when the realm lists a characters
    database (accounts whose last login came from somewhere other than the server itself, so bots are left out).
  - `GET /api/news` serves `clientPatcher/news.json`, the launcher's news, so a post is written once.
  - `POST /api/register` creates the account in every database listed under `accounts`.
- `lib/srp6.js`: the salt and verifier, as the auth server computes them.
- `tools/checkLogin.js <host> <user> <password>`: logs in like the game client, to check that an account works.

## Account creation

- Names are 3 to 16 letters or digits, and passwords 6 to 16 printable characters: these are the client's limits.
  Both are stored upper-cased, as `AccountMgr::CreateAccount` does.
- Anti-abuse: a hidden field only robots fill in (they are told it worked), and at most `signupsPerHour` accounts per
  address. Cloudflare Turnstile is used when `turnstile.siteKey` and `secretKey` are set; put it on before the site
  is public.
- Behind nginx, set `trustProxy: true` so the limit counts the visitor's address, not nginx's.
