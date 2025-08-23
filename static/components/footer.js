const footerTemplate = document.createElement('template');
footerTemplate.innerHTML = `
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

        footer{
            width:100%;
            height: 60px;
            position: absolute;
            bottom: 0;
            display:inline-block;
        }
        hr{
            margin: 0;
        }
        h1{
            color: #346734;
            font-family: FreeSerif;
        }
    </style>
    <footer>
        <hr>
        <div style="display:flex;justify-content:center;align-items:center">
            <h1>Wisdurm.fi</h1>
        </div>
    </footer>
`;


class Footer extends HTMLElement {
  constructor() {
    super();
  }

  connectedCallback() {
    const shadowRoot = this.attachShadow({mode : "closed" });
    shadowRoot.appendChild(footerTemplate.content);
  }
}

customElements.define('footer-component', Footer);