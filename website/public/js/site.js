// The pages' behaviour: the top bar, the realm status, the news, the sign-up form
(() => {
    const $ = (selector, root = document) => root.querySelector(selector);
    const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
    const page = document.body.dataset.page;

    const el = (tag, attrs = {}, children = []) => {
        const node = document.createElement(tag);
        for (const [key, value] of Object.entries(attrs)) {
            if (key === 'class') node.className = value;
            else if (key === 'text') node.textContent = value;
            else node.setAttribute(key, value);
        }
        for (const child of [].concat(children)) if (child !== null && child !== undefined) node.append(child);
        return node;
    };

    // Top bar --------------------------------------------------------------------------------------------------

    const bar = $('#header');
    $$(`[data-nav="${page}"]`).forEach((link) => link.classList.add('active'));
    $('.menu-toggle').addEventListener('click', (event) => {
        const open = bar.classList.toggle('open');
        event.currentTarget.setAttribute('aria-expanded', String(open));
    });
    $$('[data-year]').forEach((node) => { node.textContent = new Date().getFullYear(); });

    // Embers rising from the braziers of the hero
    const embers = $('.embers');
    if (embers && !matchMedia('(prefers-reduced-motion: reduce)').matches) {
        for (let i = 0; i < 26; i++) {
            const ember = el('span');
            const size = 1.5 + Math.random() * 2.5;
            ember.style.cssText = `left:${Math.random() * 100}%;width:${size}px;height:${size}px;` +
                `animation-duration:${9 + Math.random() * 12}s;animation-delay:${-Math.random() * 20}s;` +
                `--sway:${(Math.random() - 0.5) * 160}px`;
            embers.append(ember);
        }
    }

    // Site settings: the launcher's address, Discord, the anti-robot key -------------------------------------------

    const settings = fetch('/api/config').then((response) => response.json()).catch(() => ({}));
    settings.then(({ launcherUrl, discordUrl }) => {
        if (launcherUrl) $$('[data-launcher]').forEach((link) => { link.href = launcherUrl; });
        if (discordUrl) {
            $$('[data-discord]').forEach((link) => {
                link.href = discordUrl;
                link.target = '_blank';
                link.rel = 'noopener';
                link.hidden = false;
            });
        }
    });

    // Realm status ---------------------------------------------------------------------------------------------

    async function showRealms() {
        const rows = $('[data-realms]');
        if (!rows) return;
        try {
            const { realms } = await (await fetch('/api/status')).json();
            rows.replaceChildren(...realms.map((realm) => el('div', { class: 'status-row', title: realm.description }, [
                el('span', { text: 'Evolutions' }),
                el('span', { class: 'realm-tag', text: `[${realm.name}]` }),
                el('span', { class: realm.online ? 'up' : 'down', text: realm.online ? 'En ligne' : 'Hors ligne' }),
            ])));
            const counted = realms.filter((realm) => realm.online && realm.players !== null);
            const players = counted.reduce((sum, realm) => sum + realm.players, 0);
            $('[data-players]').textContent = counted.length
                ? `${players} joueur${players > 1 ? 's' : ''} en ligne` : 'État des royaumes';
        } catch {
            rows.replaceChildren(el('div', { class: 'status-row' }, el('span', { text: 'État indisponible' })));
        }
    }
    showRealms();
    setInterval(showRealms, 30000);

    // News -----------------------------------------------------------------------------------------------------

    const french = new Intl.DateTimeFormat('fr-FR', { day: 'numeric', month: 'long', year: 'numeric' });
    const dateText = (date) => french.format(new Date(`${date}T12:00:00`));
    const picture = (item) => `/img/news-${item.Image || 'evolution'}.jpg`;
    const anchor = (item) => `news-${item.Slug}`;
    const paragraphs = (body) => (body || '').split(/\n\s*\n/).map((block) => block.trim()).filter(Boolean);

    // A post's body: paragraphs split by a blank line, "- " and "• " lines as a list
    function bodyNodes(blocks) {
        return blocks.map((block) => {
            const lines = block.split('\n').map((line) => line.trim()).filter(Boolean);
            if (lines.every((line) => /^[-•]\s/.test(line))) {
                return el('ul', {}, lines.map((line) => el('li', { text: line.replace(/^[-•]\s+/, '') })));
            }
            return el('p', { text: lines.join(' ') });
        });
    }

    // A post on the home page: its first paragraph, the rest opened in place
    function newsRow(item) {
        const blocks = paragraphs(item.Body);
        const first = el('p', { text: (blocks[0] || '').replace(/\s+/g, ' ') });
        const row = el('article', { class: 'news-row', id: anchor(item) }, [
            el('div', { class: 'pic' }, el('img', { src: picture(item), alt: '', loading: 'lazy' })),
            el('div', {}, [
                el('div', { class: 'news-meta' }, [el('span', { class: 'tag', text: item.Tag }), dateText(item.Date)]),
                el('h3', { text: item.Title }),
                first,
            ]),
        ]);
        // More than its first paragraph, or one long enough to be cut at three lines
        if (blocks.length > 1 || (blocks[0] || '').length > 230) {
            const full = el('div', { class: 'news-full', hidden: '' }, bodyNodes(blocks));
            const toggle = el('button', { class: 'news-toggle', type: 'button', text: 'Lire la suite' });
            row.classList.add('opens');
            row.lastChild.append(full, toggle);
            row.open = (open = full.hidden) => {
                full.hidden = !open;
                first.hidden = open;
                toggle.textContent = open ? 'Réduire' : 'Lire la suite';
            };
            row.addEventListener('click', (event) => {
                if (event.target.closest('a')) return;
                row.open();
            });
        }
        return row;
    }

    const spot = (item) => {
        const link = el('a', { class: 'spot', href: `#${anchor(item)}` }, [
            el('div', { class: 'spot-pic' }, el('img', { src: picture(item), alt: '' })),
            el('div', { class: 'spot-title', text: item.Title }),
        ]);
        link.addEventListener('click', () => document.getElementById(anchor(item))?.open?.(true));
        return link;
    };

    fetch('/api/news').then((response) => response.json()).then(({ news }) => {
        const home = $('[data-news]');
        if (home) home.replaceChildren(...news.slice(0, Number(home.dataset.news)).map(newsRow));
        const spots = $('[data-spots]');
        if (spots) spots.replaceChildren(...news.slice(0, 2).map(spot));
        // Arrived from an old news link or a thumbnail: that post, opened
        const target = location.hash && document.getElementById(location.hash.slice(1));
        if (target?.open) {
            target.open(true);
            target.scrollIntoView();
        }
    });

    // Sign-up --------------------------------------------------------------------------------------------------

    const signup = $('[data-signup] form');
    if (signup) {
        let turnstileToken = '';
        settings.then(({ turnstileSiteKey }) => {
            if (!turnstileSiteKey) return;
            window.onTurnstile = () => window.turnstile.render('[data-turnstile]', {
                sitekey: turnstileSiteKey,
                theme: 'dark',
                callback: (token) => { turnstileToken = token; },
            });
            document.head.append(el('script', {
                src: 'https://challenges.cloudflare.com/turnstile/v0/api.js?onload=onTurnstile', async: '',
            }));
        });

        $('.reveal', signup).addEventListener('click', (event) => {
            const input = $('#password');
            input.type = input.type === 'password' ? 'text' : 'password';
            event.currentTarget.setAttribute('aria-label',
                input.type === 'password' ? 'Afficher le mot de passe' : 'Masquer le mot de passe');
        });

        const message = $('.form-message', signup);
        const setError = (field, text) => {
            const box = $(`[data-field="${field}"]`, signup);
            if (!box) return;
            box.classList.toggle('invalid', Boolean(text));
            $('.error', box).textContent = text || '';
        };

        // The server's rules, so most mistakes show before sending
        const checks = {
            username: (value) => (/^[A-Za-z0-9]{3,16}$/.test(value) ? '' : '3 à 16 lettres ou chiffres.'),
            email: (value) => (/^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/.test(value) ? '' : 'Adresse e-mail invalide.'),
            password: (value) => (/^[\x21-\x7E]{6,16}$/.test(value) ? '' : '6 à 16 caractères, sans espace ni accent.'),
            confirm: (value) => (value === $('#password').value ? '' : 'Les mots de passe ne correspondent pas.'),
        };
        for (const name of Object.keys(checks)) {
            const input = signup.elements[name];
            input.addEventListener('blur', () => { if (input.value) setError(name, checks[name](input.value)); });
            input.addEventListener('input', () => {
                if ($(`[data-field="${name}"]`, signup).classList.contains('invalid')) setError(name, checks[name](input.value));
            });
        }

        signup.addEventListener('submit', async (event) => {
            event.preventDefault();
            message.classList.remove('shown');
            let valid = true;
            for (const [name, check] of Object.entries(checks)) {
                const error = check(signup.elements[name].value);
                setError(name, error);
                if (error) valid = false;
            }
            if (!valid) return;

            const button = $('button[type="submit"]', signup);
            button.disabled = true;
            button.classList.add('busy');
            try {
                const response = await fetch('/api/register', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ ...Object.fromEntries(new FormData(signup)), token: turnstileToken }),
                });
                const result = await response.json();
                if (result.ok) {
                    $('[data-signup]').hidden = true;
                    $('[data-account-name]').textContent = result.username;
                    $('[data-success]').classList.add('shown');
                    return;
                }
                if (result.field) setError(result.field, result.message);
                else {
                    message.textContent = result.message;
                    message.classList.add('shown');
                }
                window.turnstile?.reset();
            } catch {
                message.textContent = 'Le serveur ne répond pas. Réessayez dans un instant.';
                message.classList.add('shown');
            } finally {
                button.disabled = false;
                button.classList.remove('busy');
            }
        });
    }
})();
