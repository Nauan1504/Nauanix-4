import express from "express";
import http from "http";
import mammoth from "mammoth";
import path from "path";
import fs from "fs";
import multer from "multer";
import { fileURLToPath } from "url";
import OpenAI from "openai";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
const server = http.createServer(app);
const PORT = 3000;

// === OpenAI (для генерации вопросов) ===
const openai = new OpenAI({
  apiKey: process.env.OPENAI_API_KEY || "привет",
});

// === Глобальное состояние ===
let questions = [];
let currentIndex = -1;
let scores = {}; // { player: points }
let roundActive = false;
let currentQuestion = null;
let timer = null;

// === Настройки ===
const upload = multer({ dest: "uploads/" });
app.use(express.json());
app.use(express.static(path.join(__dirname, "public")));

// === 📄 Загрузка Word-файла ===
app.post("/upload", upload.single("file"), async (req, res) => {
  try {
    const { path: filePath } = req.file;
    const buffer = fs.readFileSync(filePath);
    const result = await mammoth.extractRawText({ buffer });
    fs.unlinkSync(filePath);

    let text = result.value;
    text = text.replace(/\r/g, "").trim();

    const blocks = text
      .split(/Вопрос:/i)
      .map((b) => b.trim())
      .filter(Boolean);

    questions = blocks.map((block) => {
      const lines = block.split(/\n+/).map((l) => l.trim()).filter(Boolean);
      const question = lines[0] || "Без вопроса";
      const options = [];

      for (const line of lines) {
        const match = line.match(/^[A-DА-Г][)\.]\s*(.+)/i);
        if (match) options.push(match[1].trim());
      }

      const ansLine = lines.find((l) => /^Ответ:/i.test(l));
      const answer =
        ansLine ? parseInt(ansLine.replace(/[^0-9]/g, ""), 10) || 1 : 1;

      return { question, options, answer };
    });

    if (!questions.length) {
      return res.json({
        loaded: false,
        error: "Не удалось найти вопросы в документе.",
      });
    }

    currentIndex = -1;
    res.json({ loaded: true, count: questions.length });
  } catch (err) {
    console.error(err);
    res.json({ loaded: false, error: err.message });
  }
});

// === 🤖 Генерация AI-вопросов ===
app.get("/generate", async (req, res) => {
  const subject = req.query.subject || "Общая тема";
  try {
    const prompt = `Создай 10 коротких вопросов для викторины по теме "${subject}".
Формат строго такой:
Вопрос: [текст]
1) [вариант 1]
2) [вариант 2]
3) [вариант 3]
4) [вариант 4]
Ответ: [номер правильного варианта]`;

    const completion = await openai.chat.completions.create({
      model: "gpt-4o-mini",
      messages: [{ role: "user", content: prompt }],
    });

    const text = completion.choices[0].message.content;
    const blocks = text.split("Вопрос:").map((b) => b.trim()).filter(Boolean);

    const newQuestions = blocks.map((block) => {
      const lines = block.split("\n").map((l) => l.trim()).filter(Boolean);
      return {
        question: lines[0],
        options: [
          lines[1]?.slice(2) || "",
          lines[2]?.slice(2) || "",
          lines[3]?.slice(2) || "",
          lines[4]?.slice(2) || "",
        ],
        answer: parseInt(lines[5]?.replace("Ответ:", "").trim(), 10) || 1,
      };
    });

    if (newQuestions.length > 0) {
      questions = newQuestions;
      currentIndex = -1;
      console.log(`🤖 AI сгенерировал ${newQuestions.length} вопросов по теме "${subject}"`);
      return res.json({ generated: true, count: newQuestions.length });
    } else {
      throw new Error("Формат ответа не распознан");
    }
  } catch (err) {
    console.error("❌ Ошибка генерации:", err);
    res.json({ generated: false, error: err.message });
  }
});

// === Следующий вопрос ===
app.get("/next", (req, res) => {
  if (!questions.length) {
    return res.json({ message: "Нет загруженных или сгенерированных вопросов." });
  }

  currentIndex++;
  if (currentIndex >= questions.length) {
    return res.json({ message: "Вопросы закончились!" });
  }

  currentQuestion = questions[currentIndex];
  roundActive = true;

  if (timer) clearTimeout(timer);
  timer = setTimeout(() => {
    roundActive = false;
    console.log(`⏰ Вопрос ${currentIndex + 1} завершён (таймер).`);
  }, 15000);

  res.json({
    questionIndex: currentIndex,
    question: currentQuestion.question,
    options: currentQuestion.options,
    time: 15,
  });
});

// === Правильный ответ для ведущего ===
app.get("/answerkey", (req, res) => {
  if (!currentQuestion)
    return res.json({ message: "Нет активного вопроса." });

  const answerIndex = currentQuestion.answer - 1;
  const correctText = currentQuestion.options[answerIndex] || "—";
  res.json({ correct: currentQuestion.answer, text: correctText });
});

// === Текущий вопрос (для ESP8266) ===
app.get("/current", (req, res) => {
  res.json({
    questionIndex: currentIndex,
    question: currentQuestion?.question || null,
    options: currentQuestion?.options || [],
  });
});

// === Приём ответов от ESP8266 ===
app.get("/answer", (req, res) => {
  const player = (req.query.player || "").trim();
  const choice = parseInt(req.query.choice, 10);

  if (!player) return res.send("no_player");
  if (!questions[currentIndex]) return res.send("no_question");
  if (![1, 2, 3, 4].includes(choice)) return res.send("invalid_choice");

  if (!scores[player]) scores[player] = 0;

  const correct = choice === questions[currentIndex].answer;
  if (correct) scores[player] += 1;

  console.log(`📩 Ответ: ${player} выбрал ${choice} (${correct ? "верно" : "ошибка"})`);
  res.send(correct ? "correct" : "wrong");
});

// === Таблица очков ===
app.get("/scores", (req, res) => {
  res.json(scores);
});

// === Сброс ===
app.get("/reset", (req, res) => {
  scores = {};
  currentIndex = -1;
  roundActive = false;
  res.send("Игра сброшена.");
});

// === Главная страница ===
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "public", "quiz_app.html"));
});

// === Запуск ===
server.listen(PORT, "0.0.0.0", () => {
  console.log(`✅ Сервер запущен: http://localhost:${PORT}`);
});
