export function renderRing(ringState) {
  const container = document.getElementById("ringVisualizer");
  container.innerHTML = "";

  ringState.forEach((lane, index) => {
    const div = document.createElement("div");
    div.className = "lane";
    div.style.backgroundColor = lane.throttled ? "#ff4444" : "#44ff44";
    div.innerHTML = `
      <strong>Lane ${index}</strong><br>
      WriteIndex: ${lane.write}<br>
      ReadIndex: ${lane.read}
    `;
    container.appendChild(div);
  });
}