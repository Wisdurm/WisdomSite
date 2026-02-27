function swapFont() {
		// More readable font
		$(".zenith").css("font-family", "Lexend");
		// Increase article readablity (is this actually better? TODO)
		$("article").css("font-size", "1.4em");
		$("article").css("line-height", "1.5em");
}

$( document ).ready(function() {
		// Add some annoyance to the submission form
		// to stop the first level of trolls:
		// 1. Losers without free time
		// 2. Losers with free time
		// 3. Programmers with free time
		// (Note that in this context "programmer" is not a synonym for loser)
		let funnies = 0;
		$('#comment').on("submit", function(e){
				if (funnies < 7) {
						e.preventDefault();
						funnies++;
						// Add extra form because funny
						const addValidation = msg => {
								let checkbox = document.createElement("input");
								checkbox.type = "checkbox";
								checkbox.required = true;
								checkbox.id = "evil" + funnies;
								checkbox.name = "evil" + funnies;
								let text = document.createElement("span");
								text.innerHTML = " " + msg + "<br>";
								const fieldset = $("#comment").find("fieldset")[0];
								fieldset.appendChild(checkbox);
								fieldset.appendChild(text);
						};
						switch (funnies) {
						case 1:
								addValidation("I *promise* I didn't write anything bad");
								break;
						case 2:
								addValidation("I promise");
								break;
						case 3:
								addValidation("Genuinely");
								break;
						case 4:
								addValidation("For real");
								break;
						case 5:
								addValidation("I wouldn't lie");
								break;
						case 6:
								$("#evil2")[0].checked = false;
								break;
						case 7:
								const evils = [
										"I didn't write anything bad but I am a spam bot",
										"I *promise* I didn't not write anything bad",
										"I \"promise\"",
										"Dubiously",
										"For'dnt real",
										"I would lie",
								];
								const i = new Date().getHours() % evils.length;
								const check = $("#evil" + i)[0];
								check.nextElementSibling.innerHTML = " " + evils[i] + "<br>";
								check.required = false;
								addValidation("One last time, are you SURE about everything?");
								break;
						}
				}
		});
});
