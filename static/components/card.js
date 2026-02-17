const cardTemplate = document.createElement('template');
cardTemplate.innerHTML = `
		<link rel="stylesheet" href="/static/css/style.css">
    <style>
        a:hover {
            box-shadow: inset 0 -2px 0 0 #346734;
        }
        div{
            outline: solid black;
        }
        h3 {
            text-align: center;
        }
        p{  
            font-weight: 700;
            text-decoration: none;
            font-size: 1.2em;
            padding: 10% 5% 0%;
            text-align: center;
        }
        span{
            font-weight: 700;
            text-decoration: none;
            font-size: 1em;
            text-align: start;
            margin-right: auto;
            margin-left: 4%;
            margin-bottom: 4%;
            margin-top: auto;
        }
    </style>
    <div style="width: 300px;height: 400px; display:inline-block; margin: 20px 10px; vertical-align: top">
        <div style="display:flex; height: 100%; width: 100%; flex-direction: column;">
            <div style="width:80%; height: 35%; margin: 10% auto 0; overflow:hidden">
                <slot name="image"><img src="https://static.wikitide.net/videogameballswiki/e/ea/Canny-cat.gif" style="width:100%; height: 100%"></slot>
            </div>
            <h3 style="font-size: 1.83em; margin-top: 0; padding-top: 5%; margin-bottom: 0"><slot name="name">Cool project</slot></h3>

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
				// Fit text
				$(document).ready(() => {
						fitText($(this)[0].children[1], 2);
						// I CANNOT BELIEVE THIS WORKS
						// on the other hand I don't think this actually does what I
						// want it to and I wasted all the time I spent on this :DDDDD
				});
		}
}

customElements.define('card-component', Card);
