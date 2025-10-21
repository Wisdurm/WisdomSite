// Ye shall be warned, I've spent HOURS on this with NEY ANY progress. Do tread lightly...

const headerTemplate = document.createElement('template');
headerTemplate.innerHTML = `
    <style>
        @font-face{
            font-family: 'FreeSerif';
            src: url(FreeSerif.otf);
        }
        .sub:hover {
            box-shadow: inset 0 -2px 0 0 #346734;
        }
        a {
            font-weight: 700;
            color: #346734;
            text-decoration: none;
            font-size: 1.2em;
            font-family: FreeSerif;
        }
        nav {
            height: 50px;
            display:flex;
        }

        .logo{
            font-size: 3.1em;
            vertical-align:bottom;
        }

        .link{
            margin: auto 15px 0 15px;
        }

        .sub{
            font-family: Lucida Console;
            font-size: 1.2em;
        }

        .right{
            display: inline-block;
            width:100%;
            text-align:right;
        }

        #daily:hover{
            box-shadow: none;
        }

        header{
            border-bottom: 3px solid #346734;
            padding-bottom: 5px;
        }
    </style>
    <header>
    <nav>
        <a href="/" class="link logo">Wisdurm.fi</a>
        <a href="/projects" class="link sub">Projects</a>
        <a href="/contact" class="link sub">Contact</a>
        <a href="/blog" class="link sub">Blog</a>
        <a class="link sub right" id="daily">Word(s) of the day: <slot name="wisdom">Bruhh</slot></a>
    </nav>
    </header>
`;


class Header extends HTMLElement {
  constructor() {
    super();
  }

  connectedCallback() {
    const shadowRoot = this.attachShadow({mode : "closed" });
    shadowRoot.appendChild(headerTemplate.content);
  }
}

customElements.define('header-component', Header);