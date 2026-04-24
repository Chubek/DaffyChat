// Assuming your JSON file (quotes.json) looks like this: 
// [{"text": "Quote 1", "author": "Author 1"}, {"text": "Quote 2", "author": "Author 2"}]

async function getRandomQuote() {
  try {
    const response = await fetch('resources/quotes.json'); // Path to your local JSON file
    const quotes = await response.json();
    
    // Pick a random index
    const randomIndex = Math.floor(Math.random() * quotes.length);
    const randomQuote = quotes[randomIndex];
    
    console.log(randomQuote.text, "-", randomQuote.author);
    return randomQuote;
  } catch (error) {
    console.error("Error fetching quotes:", error);
  }
}

getRandomQuote();
