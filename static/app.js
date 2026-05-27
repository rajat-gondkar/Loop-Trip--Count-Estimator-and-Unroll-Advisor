const analyzeButton = document.querySelector("#analyzeButton");
const loadTestsButton = document.querySelector("#loadTestsButton");
const codeInput = document.querySelector("#codeInput");
const lineNumbers = document.querySelector("#lineNumbers");
const resultsBody = document.querySelector("#resultsBody");
const statusText = document.querySelector("#statusText");
const errorBox = document.querySelector("#errorBox");

const SIX_TESTS_SOURCE = `void simple_for(int *a) {
  for (int i = 0; i < 16; i++) a[i] = i * 2;
}

void large_trip(float *a, float *b) {
  for (int i = 0; i < 1024; i++) a[i] = b[i] * 3.14f;
}

void unknown_bound(int *a, int n) {
  for (int i = 0; i < n; i++) a[i] += 1;
}

void nested_loop(int m[4][4]) {
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) m[i][j] = i + j;
}

typedef struct Node { int val; struct Node *next; } Node;
int while_loop(Node *head) {
  int sum = 0;
  while (head) { sum += head->val; head = head->next; }
  return sum;
}

void symbolic(int *a, int start, int end, int step) {
  for (int i = start; i < end; i += step) a[i] = i;
}`;

function badgeClass(recommendation) {
  if (recommendation === "unroll fully") return "full";
  if (recommendation === "unroll x4") return "x4";
  return "none";
}

function setError(message) {
  errorBox.hidden = !message;
  errorBox.textContent = message || "";
}

function updateLineNumbers() {
  const lineCount = codeInput.value.split("\n").length;
  lineNumbers.textContent = Array.from({ length: lineCount }, (_, index) => index + 1).join("\n");
}

function syncLineNumberScroll() {
  lineNumbers.scrollTop = codeInput.scrollTop;
}

function renderRows(loops) {
  resultsBody.innerHTML = "";

  if (!loops.length) {
    const row = document.createElement("tr");
    row.className = "empty-row";
    row.innerHTML = '<td colspan="5">No loops were found in this input.</td>';
    resultsBody.appendChild(row);
    return;
  }

  for (const loop of loops) {
    const row = document.createElement("tr");
    const count = loop.tripCount === null
      ? '<span title="ScalarEvolution could not determine a static trip count.">-</span>'
      : String(loop.tripCount);

    row.innerHTML = `
      <td>${escapeHtml(loop.location)}</td>
      <td>${count}</td>
      <td>${escapeHtml(loop.classification)}</td>
      <td><span class="badge ${badgeClass(loop.recommendation)}">${escapeHtml(loop.recommendation)}</span></td>
      <td>${escapeHtml(loop.rationale)}</td>
    `;
    resultsBody.appendChild(row);
  }
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

async function analyzeCurrentCode() {
  analyzeButton.disabled = true;
  loadTestsButton.disabled = true;
  statusText.textContent = "Analyzing...";
  setError("");

  try {
    const response = await fetch("/analyze", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ code: codeInput.value }),
    });
    const data = await response.json();

    if (!response.ok || data.error) {
      throw new Error(data.error || "Analysis failed.");
    }

    renderRows(data.loops || []);
    statusText.textContent = `${(data.loops || []).length} loop${(data.loops || []).length === 1 ? "" : "s"} found`;
  } catch (error) {
    setError(error.message);
    statusText.textContent = "Error";
  } finally {
    analyzeButton.disabled = false;
    loadTestsButton.disabled = false;
  }
}

analyzeButton.addEventListener("click", analyzeCurrentCode);

loadTestsButton.addEventListener("click", async () => {
  codeInput.value = SIX_TESTS_SOURCE;
  updateLineNumbers();
  syncLineNumberScroll();
  await analyzeCurrentCode();
});

codeInput.addEventListener("input", updateLineNumbers);
codeInput.addEventListener("scroll", syncLineNumberScroll);
updateLineNumbers();
