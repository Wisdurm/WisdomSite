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
<p class="mobil-copyright">
Background by Setaphong Tantanawat, CC BY-SA 4.0, via Wikimedia Commons
</p>
    </footer>
`;
		}
}

customElements.define('footer-component', Footer);
