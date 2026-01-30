// Ye shall be warned, I've spent HOURS on this with NEY ANY progress. Do tread lightly...

// Fear not, for the olden times are long gone now, and an era of light
// shall spread across the lands..

class Header extends HTMLElement {
  constructor() {
    super();
  }

  connectedCallback() {
			this.innerHTML = `
    <header>
    <nav>
        <a href="/" class="link logo">Wisdurm.fi</a>
        <a href="/projects" class="sub">Projects</a>
        <a href="/contact" class="sub">Contact</a>
        <a href="/blog" class="sub">Blog</a>
    </nav>
    <div class="flex-right" id="daily-container">
        <p id="daily">
          Word(s) of the day: <slot name="wisdom">Bruhh</slot>
        </p>
    </div>
    </header>
`;
  }
}

customElements.define('header-component', Header);
