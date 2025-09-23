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
            display: flex;
            align-items: center;
            justify-content: start;
            background-color: #fff;
        }

        hr {
            color: #346734;
        }

        .logo{
            font-size: 3.1em;
        }

        .link{
            margin: auto 15px 0 15px;
        }

        .sub{
            font-family: Lucida Console;
            font-size: 1.2em;
        }

        #daily:hover{
            box-shadow: none;
        }
    </style>
    <header>
    <nav>
        <a href="/" class="link logo">Wisdurm.fi</a>
        <a href="/projects" class="link sub">Projects</a>
        <a href="/contact" class="link sub">Contact</a>
        <a href="/blog" class="link sub">Blog</a>
        <a class="link sub" style="margin-left: auto; margin-right: 50px;" id="daily">Word(s) of the day: <slot name="wisdom">Bruhh</slot></a>
    </nav>
    <hr>
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