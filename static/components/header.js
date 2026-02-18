// Ye shall be warned, I've spent HOURS on this with NEY ANY progress. Do tread lightly...

// Fear not, for the olden times are long gone now, and an era of light
// shall spread across the lands..

const headerTemplate = document.createElement('template');
headerTemplate.innerHTML = `
		<link rel="stylesheet" href="/static/css/style.css">
    <header>
<div class="border">
    <nav>
        <a href="/" class="link logo">Wisdurm.fi</a>
        <a href="/projects" class="sub">Projects</a>
        <a href="/contact" class="sub">Contact</a>
        <a href="/blog" class="sub">Blog</a>
    </nav>
</div>
<div class="border">
    <div class="flex-right" id="daily-container">
        <p id="daily">
          Word(s) of the day: <slot name="wisdom">NO SLOT</slot>
        </p>
    </div>
    <button class="important" onclick="swapFont()">Readability features</button>
</div>
    </header>
`;

class Header extends HTMLElement {
  constructor() {
			super();
  }

  connectedCallback() {
			const shadowRoot = this.attachShadow({mode : "closed" });
			shadowRoot.appendChild(headerTemplate.content.cloneNode(true));
  }
}

customElements.define('header-component', Header);
