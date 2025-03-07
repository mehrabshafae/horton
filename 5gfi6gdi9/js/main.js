"use strict";

const fs = require("fs");
const path = require("path");
const os = require("os");
const fetch = require("node-fetch");
const math = require("mathjs");

// ----- Global constants and variables -----

const CapitalTag = "capital";
const AreaTag = "area";
const JokesTag = "jokes";
const CurrencyTag = "currency";
const MathTag = "math";
const RandomTag = "random number";
const AdvicesTag = "advices";
const MoviesTag = "movies search";
const MoviesDataTag = "movies search from data";

// In-memory storage for countries, movies, messages and user info
let countries = loadCountries();
let movies = loadMovies();
let messages = {}; // Can be loaded from a JSON file if needed.
let userInformation = {}; // Keyed by user token

// ----- Utility functions -----

function getResDir(dir, file, subdir = "") {
  // Assumes resources are stored in the user's home directory under .marboris/res/
  const homeDir = os.homedir();
  if (subdir) {
    return path.join(homeDir, ".marboris", "res", dir, subdir, file);
  }
  return path.join(homeDir, ".marboris", "res", dir, file);
}

function readFile(filePath) {
  try {
    return fs.readFileSync(filePath, "utf8");
  } catch (err) {
    throw new Error("Error reading file: " + err);
  }
}

// ----- Countries functions -----

function loadCountries() {
  const filePath = getResDir("datasets", "countries.json");
  try {
    const content = readFile(filePath);
    return JSON.parse(content);
  } catch (err) {
    console.error(err);
    return [];
  }
}

function findCountry(locale, sentence) {
  sentence = sentence.toLowerCase();
  for (let country of countries) {
    let countryName = country.name[locale];
    if (!countryName) continue;
    if (sentence.includes(countryName.toLowerCase())) {
      return country;
    }
  }
  return null;
}

function areaReplacer(locale, entry, responseFormat) {
  const country = findCountry(locale, entry);
  if (!country || !country.area) {
    return { tag: "no country", message: "Country not found" };
  }
  // For example: responseFormat might be "The area of %s is %gkm²"
  return {
    tag: AreaTag,
    message: responseFormat
      .replace("%s", country.name[locale])
      .replace("%g", country.area),
  };
}

function capitalReplacer(locale, entry, responseFormat) {
  const country = findCountry(locale, entry);
  if (!country || !country.capital) {
    return { tag: "no country", message: "Country not found" };
  }
  return {
    tag: CapitalTag,
    message: responseFormat
      .replace("%s", country.name[locale])
      .replace("%s", country.capital),
  };
}

function currencyReplacer(locale, entry, responseFormat) {
  const country = findCountry(locale, entry);
  if (!country || !country.currency) {
    return { tag: "no country", message: "Country not found" };
  }
  return {
    tag: CurrencyTag,
    message: responseFormat
      .replace("%s", country.name[locale])
      .replace("%s", country.currency),
  };
}

// ----- Joke and Advice replacers (using external APIs) -----

async function jokesReplacer(locale, entry, responseFormat) {
  try {
    const res = await fetch("https://official-joke-api.appspot.com/random_joke");
    const joke = await res.json();
    const jokeStr = `${joke.setup} ${joke.punchline}`;
    return { tag: JokesTag, message: responseFormat.replace("%s", jokeStr) };
  } catch (err) {
    return { tag: "no jokes", message: "No jokes available" };
  }
}

async function advicesReplacer(locale, entry, responseFormat) {
  try {
    const res = await fetch("https://api.adviceslip.com/advice");
    const data = await res.json();
    const advice = data.slip.advice;
    return { tag: AdvicesTag, message: responseFormat.replace("%s", advice) };
  } catch (err) {
    return { tag: "no advices", message: "No advices available" };
  }
}

// ----- Math replacer -----

function findMathOperation(entry) {
  // Simplified regex to extract a math expression from the string.
  const match = entry.match(/([\d\+\-\*\/\.\(\)]+)/);
  return match ? match[0] : "";
}

function mathReplacer(locale, entry, responseFormat) {
  const operation = findMathOperation(entry);
  if (!operation) {
    return {
      tag: "don't understand",
      message: "Could not understand the math operation",
    };
  }
  try {
    const result = math.evaluate(operation);
    return {
      tag: MathTag,
      message: responseFormat.replace("%s", result.toString()),
    };
  } catch (err) {
    return { tag: "math not valid", message: "Invalid math operation" };
  }
}

// ----- Random number replacer -----

function findRangeLimits(entry) {
  const matches = entry.match(/\d+/g);
  if (!matches || matches.length < 2) {
    throw new Error("Need 2 numbers for range");
  }
  let limits = matches.slice(0, 2).map(Number).sort((a, b) => a - b);
  return limits;
}

function randomNumberReplacer(locale, entry, responseFormat) {
  try {
    const [min, max] = findRangeLimits(entry);
    const randNum = Math.floor(Math.random() * (max - min)) + min;
    return {
      tag: RandomTag,
      message: responseFormat.replace("%s", randNum.toString()),
    };
  } catch (err) {
    return { tag: "no random range", message: "No valid range provided" };
  }
}

// ----- User information (name getter/setter) -----

function getUserInformation(token) {
  return userInformation[token] || { name: "", movie_genres: [], movie_blacklist: [] };
}

function changeUserInformation(token, changer) {
  userInformation[token] = changer(getUserInformation(token));
}

function loadNames() {
  const filePath = getResDir("datasets", "names.txt");
  try {
    const content = readFile(filePath);
    return content.trim().split("\n");
  } catch (err) {
    return [];
  }
}

const names = loadNames();

function findName(sentence) {
  sentence = sentence.toLowerCase();
  for (let name of names) {
    if (sentence.includes(name.toLowerCase())) {
      return name;
    }
  }
  return "";
}

function nameGetterReplacer(locale, responseFormat, token) {
  const info = getUserInformation(token);
  if (!info.name) {
    return { tag: "don't know name", message: "Name is not set" };
  }
  return { tag: "name getter", message: responseFormat.replace("%s", info.name) };
}

function nameSetterReplacer(locale, entry, responseFormat, token) {
  let name = findName(entry);
  if (!name) {
    return { tag: "no name", message: "No valid name found" };
  }
  name = name.charAt(0).toUpperCase() + name.slice(1);
  changeUserInformation(token, (info) => ({ ...info, name }));
  return { tag: "name setter", message: responseFormat.replace("%s", name) };
}

// ----- Movies and Genres functions -----

function loadMovies() {
  const filePath = getResDir("datasets", "movies.csv");
  try {
    const content = readFile(filePath);
    const lines = content.split("\n");
    const result = [];
    // Assuming CSV columns: id,name,genres,rating
    for (let i = 1; i < lines.length; i++) {
      const line = lines[i].trim();
      if (!line) continue;
      const parts = line.split(",");
      const rating = parseFloat(parts[3]);
      result.push({
        name: parts[1],
        genres: parts[2].split("|"),
        rating: rating,
      });
    }
    return result;
  } catch (err) {
    return [];
  }
}

function findMoviesGenres(locale, content) {
  // Simplified implementation: check against a preset list of genres.
  const genresList = ["Action", "Adventure", "Comedy", "Horror", "Drama"];
  let found = [];
  for (let genre of genresList) {
    if (content.toLowerCase().includes(genre.toLowerCase())) {
      found.push(genre);
    }
  }
  return found;
}

function searchMovie(genre, token) {
  const info = getUserInformation(token);
  let movie = null;
  for (let m of movies) {
    if (m.genres.includes(genre) && !(info.movie_blacklist || []).includes(m.name)) {
      if (!movie || m.rating > movie.rating) {
        movie = m;
      }
    }
  }
  // Add found movie to user's blacklist so it isn’t repeated
  changeUserInformation(token, (info) => {
    const blacklist = info.movie_blacklist || [];
    if (movie) blacklist.push(movie.name);
    return { ...info, movie_blacklist: blacklist };
  });
  return movie;
}

function movieSearchReplacer(locale, entry, responseFormat, token) {
  const genres = findMoviesGenres(locale, entry);
  if (genres.length === 0) {
    return { tag: "no genres", message: "No valid genres found" };
  }
  const movie = searchMovie(genres[0], token);
  if (!movie) {
    return { tag: "no movie", message: "No movie found" };
  }
  return {
    tag: MoviesTag,
    message: responseFormat
      .replace("%s", movie.name)
      .replace("%.02f", movie.rating.toFixed(2)),
  };
}

function movieSearchFromInformationReplacer(locale, entry, responseFormat, token) {
  const info = getUserInformation(token);
  if (!info.movie_genres || info.movie_genres.length === 0) {
    return { tag: "no genres saved", message: "No genres saved" };
  }
  const randomGenre =
    info.movie_genres[Math.floor(Math.random() * info.movie_genres.length)];
  const movie = searchMovie(randomGenre, token);
  if (!movie) {
    return { tag: "no movie", message: "No movie found" };
  }
  const genresJoined = info.movie_genres.join(", ");
  return {
    tag: MoviesDataTag,
    message: responseFormat
      .replace("%s", genresJoined)
      .replace("%s", movie.name)
      .replace("%.02f", movie.rating.toFixed(2)),
  };
}

// ----- A simple Neural Network implementation -----

class NeuralNetwork {
  constructor(locale, rate, inputs, outputs, hiddenNodes) {
    this.locale = locale;
    this.rate = rate;
    this.inputs = inputs; // matrix (array of arrays)
    this.outputs = outputs; // matrix (array of arrays)
    this.hiddenNodes = hiddenNodes;
    // Create layers: input, hidden, output
    this.layers = [];
    this.layers.push(inputs);
    this.layers.push(NeuralNetwork.createMatrix(inputs.length, hiddenNodes));
    this.layers.push(outputs);
    // Initialize weights and biases with random values
    this.weights = [
      NeuralNetwork.randomMatrix(inputs[0].length, hiddenNodes),
      NeuralNetwork.randomMatrix(hiddenNodes, outputs[0].length),
    ];
    this.biases = [
      NeuralNetwork.randomMatrix(inputs.length, hiddenNodes),
      NeuralNetwork.randomMatrix(inputs.length, outputs[0].length),
    ];
  }

  static createMatrix(rows, columns) {
    let matrix = [];
    for (let i = 0; i < rows; i++) {
      matrix.push(new Array(columns).fill(0));
    }
    return matrix;
  }

  static randomMatrix(rows, columns) {
    let matrix = [];
    for (let i = 0; i < rows; i++) {
      let row = [];
      for (let j = 0; j < columns; j++) {
        row.push(Math.random() * 2 - 1);
      }
      matrix.push(row);
    }
    return matrix;
  }

  static dotProduct(a, b) {
    let result = NeuralNetwork.createMatrix(a.length, b[0].length);
    for (let i = 0; i < a.length; i++) {
      for (let j = 0; j < b[0].length; j++) {
        let sum = 0;
        for (let k = 0; k < a[0].length; k++) {
          sum += a[i][k] * b[k][j];
        }
        result[i][j] = sum;
      }
    }
    return result;
  }

  static sigmoid(x) {
    return 1 / (1 + Math.exp(-x));
  }

  static applyFunction(matrix, fn) {
    return matrix.map((row) => row.map((val) => fn(val)));
  }

  feedForward() {
    // Input -> Hidden
    let hiddenInput = NeuralNetwork.dotProduct(this.inputs, this.weights[0]);
    hiddenInput = hiddenInput.map((row, i) =>
      row.map((val, j) => val + (this.biases[0][i][j] || 0))
    );
    const hiddenOutput = NeuralNetwork.applyFunction(hiddenInput, NeuralNetwork.sigmoid);
    this.layers[1] = hiddenOutput;
    // Hidden -> Output
    let finalInput = NeuralNetwork.dotProduct(hiddenOutput, this.weights[1]);
    finalInput = finalInput.map((row, i) =>
      row.map((val, j) => val + (this.biases[1][i][j] || 0))
    );
    const finalOutput = NeuralNetwork.applyFunction(finalInput, NeuralNetwork.sigmoid);
    this.layers[2] = finalOutput;
  }

  // A very simplified training loop (without full backpropagation)
  train(iterations) {
    for (let i = 0; i < iterations; i++) {
      this.feedForward();
      // Normally, compute error, calculate gradients, and update weights/biases.
      // Here we omit the full derivative calculations for brevity.
    }
  }
}

// ----- Export the functions and classes -----

// module.exports = {
//   areaReplacer,
//   capitalReplacer,
//   currencyReplacer,
//   jokesReplacer,
//   mathReplacer,
//   randomNumberReplacer,
//   advicesReplacer,
//   nameGetterReplacer,
//   nameSetterReplacer,
//   movieSearchReplacer,
//   movieSearchFromInformationReplacer,
//   NeuralNetwork,
//   getUserInformation,
//   changeUserInformation,
// };
const result = mathReplacer("en", "Calculate 3+6*2", "The result is %s");
console.log(result);
// (async () => {
//     //     const result = await jokesReplacer("en", "Tell me a joke", "Here's one: %s");
//     // console.log(result);
//     const userToken = "user123";

//     const setNameResult = nameSetterReplacer("en", "My name is Alice", "Great! Hi %s", userToken);
// console.log(setNameResult);
//   })();