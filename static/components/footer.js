class Footer extends HTMLElement {
  constructor() {
    super();
  }

		connectedCallback() {
				this.innerHTML =  `
    <footer>
        <h1>Wisdurm.fi</h1>
			  <p>
          Licensed under CC0-1.0;
          do whatever you want •
          <a href="https://github.com/Wisdurm/WisdomSite">Source code</a>
        <p>
    </footer>
`;
		}
}

customElements.define('footer-component', Footer);
