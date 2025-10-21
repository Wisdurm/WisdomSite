const cardTemplate = document.createElement('template');
cardTemplate.innerHTML = `
    <style>
        @font-face{
        font-family: 'FreeSerif';
        src: url(FreeSerif.otf);
        }
        a:hover {
            box-shadow: inset 0 -2px 0 0 #346734;
        }
        div{
            outline: solid #346734;
        }
        p{  
            font-weight: 700;
            color: #346734;
            text-decoration: none;
            font-size: 1.2em;
            padding: 10% 5% 0%;
            text-align: center;
        }
        span{
            font-weight: 700;
            color: #346734;
            text-decoration: none;
            font-size: 0.94em;
            text-align: start;
            margin-right: auto;
            margin-left: 4%;
            margin-bottom: 4%;
            margin-top: auto;
        }
    </style>
    <div style="width: 320px;height: 400px; display:inline-block; margin-right: 20px; vertical-align: top">
        <div style="display:flex; height: 100%; width: 100%; flex-direction: column;">
            <div style="width:80%; height: 40%; margin: 8% auto 0; overflow:hidden">
                <slot name="image"><img src="https://static.wikitide.net/videogameballswiki/e/ea/Canny-cat.gif" style="width:100%; height: 100%"></slot>
            </div>
            <p style="font-size: 1.83em; margin-top: 0; padding-top: 5%; margin-bottom: 0"><slot name="name">Cool project</slot></p>

            <p style="font-size: 0.95em; padding-top: 0; margin-top: 3%">
            <slot name="text">
            This sure is a card. \n
            Lorem ipsum dolor sit amet consectitur disapinis i not rember corec tisus soemon helepa mis
            </slot></p>

            <span>Link(s): <slot name="links"><a href="https://www.google.com">Google</a></slot></span>
        </div>
    </div>
`;


class Card extends HTMLElement {
  constructor() {
    super();
  }

  connectedCallback() {
    const shadowRoot = this.attachShadow({mode : "closed" });
    shadowRoot.appendChild(cardTemplate.content.cloneNode(true));
  }
}

customElements.define('card-component', Card);