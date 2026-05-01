const analyzeButton = document.querySelector("#analyzeButton");
const codeInput = document.querySelector("#codeInput");
const resultsBody = document.querySelector("#resultsBody");
const statusText = document.querySelector("#statusText");
const errorBox = document.querySelector("#errorBox");

function badgeClass(recommendation) {
  if (recommendation === "unroll fully") return "full";
  if (recommendation === "unroll x4") return "x4";
  return "none";
}

function setError(message) {
  errorBox.hidden = !message;
  errorBox.textContent = message || "";
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

analyzeButton.addEventListener("click", async () => {
  analyzeButton.disabled = true;
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
  }
});
