class Footer extends HTMLElement {
  constructor() {
    super();
  }

		connectedCallback() {
				this.innerHTML =  `
    <footer>
        <h1>Wisdurm.fi</h1>
			  <p>
          Licensed under the BSD-2-Clause license
          <a href="https://github.com/Wisdurm/WisdomSite">• Source code</a>
        </p>
    </footer>
`;
		}
}

customElements.define('footer-component', Footer);
