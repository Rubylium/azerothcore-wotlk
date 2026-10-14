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
    const link = (item) => `/nouveautes/${item.Slug}`;
    const excerpt = (body) => (body || '').split(/\n\s*\n/)[0].replace(/\s+/g, ' ').trim();

    // A post's body: paragraphs split by a blank line, "- " and "• " lines as a list
    function bodyNodes(body) {
        const nodes = [];
        for (const block of (body || '').split(/\n\s*\n/)) {
            const lines = block.split('\n').map((line) => line.trim()).filter(Boolean);
            if (!lines.length) continue;
            if (lines.every((line) => /^[-•]\s/.test(line))) {
                nodes.push(el('ul', {}, lines.map((line) => el('li', { text: line.replace(/^[-•]\s+/, '') }))));
            } else {
                nodes.push(el('p', { text: lines.join(' ') }));
            }
        }
        return nodes;
    }

    const newsRow = (item) => el('a', { class: 'news-row', href: link(item) }, [
        el('div', { class: 'pic' }, el('img', { src: picture(item), alt: '', loading: 'lazy' })),
        el('div', {}, [
            el('div', { class: 'news-meta' }, [el('span', { class: 'tag', text: item.Tag }), dateText(item.Date)]),
            el('h3', { text: item.Title }),
            el('p', { text: excerpt(item.Body) }),
        ]),
    ]);

    const spot = (item) => el('a', { class: 'spot', href: link(item) }, [
        el('div', { class: 'spot-pic' }, el('img', { src: picture(item), alt: '' })),
        el('div', { class: 'spot-title', text: item.Title }),
    ]);

    const newsData = fetch('/api/news').then((response) => response.json()).then((data) => data.news);

    newsData.then((items) => {
        const spots = $('[data-spots]');
        if (spots) spots.replaceChildren(...items.slice(0, 2).map(spot));
        const home = $('[data-news]');
        if (home) home.replaceChildren(...items.slice(0, Number(home.dataset.news)).map(newsRow));
        if ($('[data-news-page]')) {
            const slug = decodeURIComponent(location.pathname.replace(/^\/nouveautes\/?/, ''));
            if (slug) showArticle(items.find((item) => item.Slug === slug));
            else showNewsList(items);
        }
    });

    function showNewsList(items) {
        const filters = $('[data-news-filters]');
        const tags = ['Toutes', ...new Set(items.map((item) => item.Tag))];
        let current = 'Toutes';
        const render = () => {
            const shown = current === 'Toutes' ? items : items.filter((item) => item.Tag === current);
            $('[data-news-all]').replaceChildren(...shown.map(newsRow));
            $$('.chip', filters).forEach((chip) => chip.classList.toggle('active', chip.textContent === current));
        };
        filters.replaceChildren(...tags.map((tag) => {
            const chip = el('button', { class: 'chip', type: 'button', text: tag });
            chip.addEventListener('click', () => { current = tag; render(); });
            return chip;
        }));
        render();
    }

    function showArticle(item) {
        if (!item) {
            location.replace('/nouveautes');
            return;
        }
        $('[data-news-head]').hidden = true;
        $('[data-news-list]').hidden = true;
        $('[data-news-article]').hidden = false;
        $('[data-article-pic]').src = picture(item);
        $('[data-article-tag]').textContent = item.Tag;
        $('[data-article-date]').textContent = dateText(item.Date);
        $('[data-article-title]').textContent = item.Title;
        $('[data-article-body]').replaceChildren(...bodyNodes(item.Body));
        document.title = `${item.Title} - Evolutions`;
    }

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
