// The pages' behaviour: the header, the realm status, the news, the sign-up form
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
        for (const child of [].concat(children)) if (child) node.append(child);
        return node;
    };

    // Header ---------------------------------------------------------------------------------------------------

    const header = $('#header');
    const onScroll = () => header.classList.toggle('scrolled', window.scrollY > 40);
    window.addEventListener('scroll', onScroll, { passive: true });
    onScroll();

    $$(`[data-nav="${page}"]`).forEach((link) => link.classList.add('active'));
    $('.menu-toggle').addEventListener('click', (event) => {
        const open = header.classList.toggle('open');
        event.currentTarget.setAttribute('aria-expanded', String(open));
    });
    $$('.nav a').forEach((link) => link.addEventListener('click', () => header.classList.remove('open')));
    $$('[data-year]').forEach((node) => { node.textContent = new Date().getFullYear(); });

    // Embers rising over the hero, as the launcher's braziers
    const embers = $('.embers');
    if (embers && !matchMedia('(prefers-reduced-motion: reduce)').matches) {
        for (let i = 0; i < 28; i++) {
            const ember = el('span');
            const size = 1.5 + Math.random() * 2.5;
            ember.style.cssText = `left:${Math.random() * 100}%;width:${size}px;height:${size}px;` +
                `animation-duration:${9 + Math.random() * 12}s;animation-delay:${-Math.random() * 20}s;` +
                `--sway:${(Math.random() - 0.5) * 160}px`;
            embers.append(ember);
        }
    }

    // Sections rise into view once
    const reveal = new IntersectionObserver((entries) => {
        for (const entry of entries) {
            if (!entry.isIntersecting) continue;
            entry.target.classList.add('visible');
            reveal.unobserve(entry.target);
        }
    }, { rootMargin: '0px 0px -8% 0px' });
    $$('.reveal-up').forEach((node) => reveal.observe(node));

    // Site settings: the launcher's address, the anti-robot key ---------------------------------------------------

    const settings = fetch('/api/config').then((response) => response.json()).catch(() => ({}));
    settings.then(({ launcherUrl }) => {
        if (launcherUrl) $$('[data-launcher]').forEach((link) => { link.href = launcherUrl; });
    });

    // Realm status ---------------------------------------------------------------------------------------------

    function realmCard(realm) {
        const state = realm.online ? 'En ligne' : 'Hors ligne';
        const players = realm.online && realm.players !== null
            ? ` · ${realm.players} joueur${realm.players === 1 ? '' : 's'}` : '';
        const name = el('div', { class: 'realm-name', text: 'Evolutions' },
            el('small', { text: realm.name.toUpperCase() }));
        const detail = el('div', { class: 'realm-state' }, [el('strong', { text: state }), players]);
        return el('div', { class: `realm ${realm.online ? 'online' : 'offline'}`, title: realm.description },
            [el('span', { class: 'realm-dot' }), el('div', {}, [name, detail])]);
    }

    async function showRealms() {
        const cards = $('[data-realms]');
        const list = $('[data-realm-list]');
        try {
            const { realms } = await (await fetch('/api/status')).json();
            if (cards) cards.replaceChildren(...realms.map(realmCard));
            if (list) {
                list.replaceChildren(...realms.map((realm) => el('li', {},
                    `${realm.name} : ${realm.online ? 'en ligne' : 'hors ligne'}`)));
            }
        } catch {
            if (list) list.replaceChildren(el('li', { text: 'État indisponible' }));
        }
    }
    showRealms();
    setInterval(showRealms, 30000);

    // News -----------------------------------------------------------------------------------------------------

    const french = new Intl.DateTimeFormat('fr-FR', { day: 'numeric', month: 'long', year: 'numeric' });
    const dateText = (date) => french.format(new Date(`${date}T12:00:00`));
    const picture = (item) => `/img/news-${item.Image || 'evolution'}.jpg`;

    // A post's body: paragraphs split by a blank line, "- " and "• " lines as a list
    function bodyNodes(body) {
        const nodes = [];
        for (const block of (body || '').split(/\n\s*\n/)) {
            const lines = block.split('\n').map((line) => line.trim()).filter(Boolean);
            if (!lines.length) continue;
            const bullets = lines.filter((line) => /^[-•]\s/.test(line));
            if (bullets.length === lines.length) {
                nodes.push(el('ul', {}, lines.map((line) => el('li', { text: line.replace(/^[-•]\s+/, '') }))));
            } else {
                nodes.push(el('p', { text: lines.join(' ') }));
            }
        }
        return nodes;
    }

    const excerpt = (body) => (body || '').split(/\n\s*\n/)[0].replace(/\s+/g, ' ').trim();

    function newsCard(item, featured = false) {
        const pic = el('div', { class: 'news-pic' }, [
            el('img', { src: picture(item), alt: '', loading: 'lazy' }),
            featured ? null : el('span', { class: 'tag', text: item.Tag }),
        ]);
        const body = el('div', { class: 'news-body' }, [
            featured ? el('div', { class: 'roles', style: 'justify-content:flex-start' }, el('span', { class: 'tag', text: item.Tag })) : null,
            el('div', { class: 'news-date', text: dateText(item.Date) }),
            el('h3', { text: item.Title }),
            el('p', { text: excerpt(item.Body) }),
            featured ? el('span', { class: 'read-more', text: 'Lire la suite →' }) : null,
        ]);
        return el('a', { class: featured ? 'news-featured' : 'news-card', href: `/nouveautes/${item.Slug}` }, [pic, body]);
    }

    const newsData = fetch('/api/news').then((response) => response.json()).then((data) => data.news);

    const homeNews = $('[data-news]');
    if (homeNews) {
        newsData.then((items) => {
            homeNews.replaceChildren(...items.slice(0, Number(homeNews.dataset.news)).map((item) => newsCard(item)));
        });
    }

    if ($('[data-news-page]')) {
        newsData.then((items) => {
            const slug = decodeURIComponent(location.pathname.replace(/^\/nouveautes\/?/, ''));
            if (slug) showArticle(items.find((item) => item.Slug === slug));
            else showNewsList(items);
        });
    }

    function showNewsList(items) {
        const filters = $('[data-news-filters]');
        const tags = ['Tout', ...new Set(items.map((item) => item.Tag))];
        let current = 'Tout';
        const render = () => {
            const shown = current === 'Tout' ? items : items.filter((item) => item.Tag === current);
            $('[data-news-featured]').replaceChildren(shown.length ? newsCard(shown[0], true) : '');
            $('[data-news-all]').replaceChildren(...shown.slice(1).map((item) => newsCard(item)));
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
        $('[data-news-banner]').style.display = 'none';
        $('[data-news-list]').style.display = 'none';
        $('[data-news-article]').style.display = '';
        $('[data-article-pic]').src = picture(item);
        $('[data-article-tag]').textContent = item.Tag;
        $('[data-article-date]').textContent = dateText(item.Date);
        $('[data-article-title]').textContent = item.Title;
        $('[data-article-body]').replaceChildren(...bodyNodes(item.Body));
        document.title = `${item.Title} - Evolutions`;
        header.classList.add('solid');
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

        // The same rules as the server, so most mistakes show before sending
        const checks = {
            username: (value) => (/^[A-Za-z0-9]{3,16}$/.test(value) ? '' : 'De 3 à 16 caractères : lettres et chiffres uniquement.'),
            email: (value) => (/^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/.test(value) ? '' : 'Adresse e-mail invalide.'),
            password: (value) => (/^[\x21-\x7E]{6,16}$/.test(value) ? '' : 'De 6 à 16 caractères, sans espace ni accent.'),
            confirm: (value) => (value === $('#password').value ? '' : 'Les deux mots de passe ne correspondent pas.'),
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
                    $('[data-signup]').style.display = 'none';
                    $('[data-account-name]').textContent = result.username;
                    $('[data-success]').classList.add('shown');
                    window.scrollTo({ top: 0, behavior: 'smooth' });
                    return;
                }
                if (result.field) setError(result.field, result.message);
                else {
                    message.textContent = result.message;
                    message.classList.add('shown');
                }
                window.turnstile?.reset();
            } catch {
                message.textContent = 'Impossible de joindre le serveur. Vérifiez votre connexion et réessayez.';
                message.classList.add('shown');
            } finally {
                button.disabled = false;
                button.classList.remove('busy');
            }
        });
    }
})();
