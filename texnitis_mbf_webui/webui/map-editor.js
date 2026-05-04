const canvas = document.getElementById('grid-canvas');
const ctx = canvas.getContext('2d');
const fileInput = document.getElementById('csv-file-input');

const ui = {
  width: document.getElementById('map-width'),
  height: document.getElementById('map-height'),
  resolution: document.getElementById('map-resolution'),
  frameId: document.getElementById('map-frame-id'),
  originX: document.getElementById('map-origin-x'),
  originY: document.getElementById('map-origin-y'),
  originYaw: document.getElementById('map-origin-yaw'),
  fileName: document.getElementById('map-file-name'),
  row0Bottom: document.getElementById('row0-bottom'),
  brushSize: document.getElementById('brush-size'),
  zoomPercent: document.getElementById('zoom-percent'),
  robotRadius: document.getElementById('robot-radius'),
  inflationRadius: document.getElementById('inflation-radius'),
  showGrid: document.getElementById('show-grid'),
  showInflation: document.getElementById('show-inflation'),
  yamlPreview: document.getElementById('yaml-preview'),
  statusLine: document.getElementById('status-line'),
  cursor: document.getElementById('metric-cursor'),
  value: document.getElementById('metric-value'),
  mapSize: document.getElementById('metric-size'),
  mapSpan: document.getElementById('metric-span'),
  occupied: document.getElementById('metric-occupied'),
  unknown: document.getElementById('metric-unknown'),
};

const state = {
  width: 48,
  height: 32,
  resolution: 0.05,
  frameId: 'map',
  originX: -1.2,
  originY: -0.8,
  originYaw: 0.0,
  fileName: 'occupancy_map.csv',
  row0Bottom: false,
  brushSize: 1,
  robotRadius: 0.22,
  inflationRadius: 0.4,
  tool: 'draw',
  drawValue: 100,
  zoom: 1,
  panX: 0,
  panY: 0,
  showGrid: true,
  showInflation: true,
  cells: [],
  history: [],
  pointer: {
    isDown: false,
    mode: null,
    startCell: null,
    currentCell: null,
    lastPaintCell: null,
    panStartX: 0,
    panStartY: 0,
    startPanX: 0,
    startPanY: 0,
  },
  hoverCell: null,
};

function createGrid(width, height, fillValue = 0) {
  return Array.from({ length: height }, () => Array(width).fill(fillValue));
}

function cloneGrid(cells) {
  return cells.map((row) => row.slice());
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function normalizeCsvFileName(name) {
  return name.toLowerCase().endsWith('.csv') ? name : `${name}.csv`;
}

function syncControlsFromState() {
  ui.width.value = state.width;
  ui.height.value = state.height;
  ui.resolution.value = state.resolution.toFixed(3);
  ui.frameId.value = state.frameId;
  ui.originX.value = state.originX.toFixed(2);
  ui.originY.value = state.originY.toFixed(2);
  ui.originYaw.value = state.originYaw.toFixed(2);
  ui.fileName.value = state.fileName;
  ui.row0Bottom.checked = state.row0Bottom;
  ui.brushSize.value = state.brushSize;
  ui.zoomPercent.value = Math.round(state.zoom * 100);
  ui.robotRadius.value = state.robotRadius.toFixed(2);
  ui.inflationRadius.value = state.inflationRadius.toFixed(2);
  ui.showGrid.checked = state.showGrid;
  ui.showInflation.checked = state.showInflation;
}

function syncStateFromControls() {
  state.resolution = clamp(parseFloat(ui.resolution.value) || 0.05, 0.001, 100.0);
  state.frameId = ui.frameId.value.trim() || 'map';
  state.originX = parseFloat(ui.originX.value) || 0.0;
  state.originY = parseFloat(ui.originY.value) || 0.0;
  state.originYaw = parseFloat(ui.originYaw.value) || 0.0;
  state.fileName = normalizeCsvFileName(ui.fileName.value.trim() || 'occupancy_map.csv');
  state.row0Bottom = ui.row0Bottom.checked;
  state.brushSize = clamp(parseInt(ui.brushSize.value, 10) || 1, 1, 16);
  state.zoom = clamp((parseFloat(ui.zoomPercent.value) || 100) / 100, 0.2, 20.0);
  state.robotRadius = Math.max(0, parseFloat(ui.robotRadius.value) || 0.0);
  state.inflationRadius = Math.max(0, parseFloat(ui.inflationRadius.value) || 0.0);
  state.showGrid = ui.showGrid.checked;
  state.showInflation = ui.showInflation.checked;
  syncControlsFromState();
}

function pushHistory() {
  state.history.push({
    width: state.width,
    height: state.height,
    cells: cloneGrid(state.cells),
  });
  if (state.history.length > 40) {
    state.history.shift();
  }
}

function undo() {
  const prev = state.history.pop();
  if (!prev) {
    setStatus('Undo history is empty.');
    return;
  }
  state.width = prev.width;
  state.height = prev.height;
  state.cells = prev.cells;
  syncControlsFromState();
  updateMetrics();
  render();
  setStatus('Reverted one edit.');
}

function setStatus(message) {
  ui.statusLine.textContent = message;
}

function setTool(tool) {
  state.tool = tool;
  document.querySelectorAll('[data-tool]').forEach((button) => {
    button.classList.toggle('active', button.dataset.tool === tool);
  });
  canvas.style.cursor = tool === 'pan' ? 'grab' : tool === 'pick' ? 'copy' : 'crosshair';
  setStatus(`Tool: ${tool}.`);
}

function setDrawValue(value) {
  state.drawValue = Number(value);
  document.querySelectorAll('[data-value]').forEach((button) => {
    button.classList.toggle('active', Number(button.dataset.value) === state.drawValue);
  });
  setStatus(`Draw value: ${formatCellValue(state.drawValue)}.`);
}

function resizeCanvas() {
  const rect = canvas.parentElement.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  canvas.width = Math.round(rect.width * dpr);
  canvas.height = Math.round(rect.height * dpr);
  canvas.style.width = `${rect.width}px`;
  canvas.style.height = `${rect.height}px`;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  render();
}

function fitViewToMap() {
  const rect = canvas.parentElement.getBoundingClientRect();
  const cellSize = Math.max(
    10,
    Math.min(
      rect.width / Math.max(1, state.width),
      rect.height / Math.max(1, state.height)
    ) * 0.88
  );
  state.zoom = cellSize / 26;
  state.panX = 0;
  state.panY = 0;
  ui.zoomPercent.value = Math.round(state.zoom * 100);
}

function getCellSizePx() {
  return 26 * state.zoom;
}

function getGridBounds() {
  const cellSize = getCellSizePx();
  const widthPx = state.width * cellSize;
  const heightPx = state.height * cellSize;
  const viewWidth = canvas.width / (window.devicePixelRatio || 1);
  const viewHeight = canvas.height / (window.devicePixelRatio || 1);
  const offsetX = viewWidth / 2 - widthPx / 2 + state.panX;
  const offsetY = viewHeight / 2 - heightPx / 2 + state.panY;
  return { cellSize, widthPx, heightPx, offsetX, offsetY, viewWidth, viewHeight };
}

function canvasPointToCell(clientX, clientY) {
  const rect = canvas.getBoundingClientRect();
  const { cellSize, offsetX, offsetY } = getGridBounds();
  const x = clientX - rect.left;
  const y = clientY - rect.top;
  const col = Math.floor((x - offsetX) / cellSize);
  const rowTop = Math.floor((y - offsetY) / cellSize);
  if (col < 0 || rowTop < 0 || col >= state.width || rowTop >= state.height) {
    return null;
  }
  const row = state.height - 1 - rowTop;
  return { x: col, y: row };
}

function applyBrushAt(cell, value = state.drawValue) {
  if (!cell) {
    return false;
  }
  const radius = Math.max(0, state.brushSize - 1);
  let changed = false;
  for (let dy = -radius; dy <= radius; dy += 1) {
    for (let dx = -radius; dx <= radius; dx += 1) {
      const nx = cell.x + dx;
      const ny = cell.y + dy;
      if (nx < 0 || ny < 0 || nx >= state.width || ny >= state.height) {
        continue;
      }
      if (state.cells[ny][nx] !== value) {
        state.cells[ny][nx] = value;
        changed = true;
      }
    }
  }
  return changed;
}

function applyRect(start, end, value = state.drawValue) {
  if (!start || !end) {
    return false;
  }
  const minX = Math.min(start.x, end.x);
  const maxX = Math.max(start.x, end.x);
  const minY = Math.min(start.y, end.y);
  const maxY = Math.max(start.y, end.y);
  let changed = false;
  for (let y = minY; y <= maxY; y += 1) {
    for (let x = minX; x <= maxX; x += 1) {
      if (state.cells[y][x] !== value) {
        state.cells[y][x] = value;
        changed = true;
      }
    }
  }
  return changed;
}

function floodFill(start, value = state.drawValue) {
  if (!start) {
    return false;
  }
  const target = state.cells[start.y][start.x];
  if (target === value) {
    return false;
  }
  const queue = [start];
  const visited = new Set([`${start.x}:${start.y}`]);
  let changed = false;
  while (queue.length > 0) {
    const current = queue.shift();
    if (state.cells[current.y][current.x] !== target) {
      continue;
    }
    state.cells[current.y][current.x] = value;
    changed = true;
    const neighbors = [
      { x: current.x + 1, y: current.y },
      { x: current.x - 1, y: current.y },
      { x: current.x, y: current.y + 1 },
      { x: current.x, y: current.y - 1 },
    ];
    for (const next of neighbors) {
      if (next.x < 0 || next.y < 0 || next.x >= state.width || next.y >= state.height) {
        continue;
      }
      const key = `${next.x}:${next.y}`;
      if (!visited.has(key) && state.cells[next.y][next.x] === target) {
        visited.add(key);
        queue.push(next);
      }
    }
  }
  return changed;
}

function getInflatedCells() {
  if (!state.showInflation || state.inflationRadius <= 0 || state.resolution <= 0) {
    return new Set();
  }
  const radiusCells = Math.ceil(state.inflationRadius / state.resolution);
  const occupiedCells = [];
  for (let y = 0; y < state.height; y += 1) {
    for (let x = 0; x < state.width; x += 1) {
      if (state.cells[y][x] >= 65) {
        occupiedCells.push({ x, y });
      }
    }
  }
  const inflated = new Set();
  for (const cell of occupiedCells) {
    for (let dy = -radiusCells; dy <= radiusCells; dy += 1) {
      for (let dx = -radiusCells; dx <= radiusCells; dx += 1) {
        const nx = cell.x + dx;
        const ny = cell.y + dy;
        if (nx < 0 || ny < 0 || nx >= state.width || ny >= state.height) {
          continue;
        }
        const distance = Math.hypot(dx, dy) * state.resolution;
        if (distance <= state.inflationRadius && state.cells[ny][nx] < 65) {
          inflated.add(`${nx}:${ny}`);
        }
      }
    }
  }
  return inflated;
}

function cellFill(value) {
  if (value < 0) {
    return '#8b5cf6';
  }
  if (value >= 65) {
    return '#f97316';
  }
  const shade = Math.round(30 + (1 - value / 100) * 30);
  return `rgb(${shade}, ${shade + 6}, ${shade + 12})`;
}

function drawRobotOverlay(bounds) {
  if (state.robotRadius <= 0 || state.resolution <= 0) {
    return;
  }
  const radiusCells = state.robotRadius / state.resolution;
  const radiusPx = radiusCells * bounds.cellSize;
  const cx = bounds.offsetX + bounds.widthPx / 2;
  const cy = bounds.offsetY + bounds.heightPx / 2;
  ctx.save();
  ctx.strokeStyle = 'rgba(0, 184, 169, 0.75)';
  ctx.fillStyle = 'rgba(0, 184, 169, 0.08)';
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.arc(cx, cy, radiusPx, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.restore();
}

function drawRectPreview(bounds) {
  const { startCell, currentCell } = state.pointer;
  if (state.tool !== 'rect' || !state.pointer.isDown || !startCell || !currentCell) {
    return;
  }
  const minX = Math.min(startCell.x, currentCell.x);
  const maxX = Math.max(startCell.x, currentCell.x);
  const minY = Math.min(startCell.y, currentCell.y);
  const maxY = Math.max(startCell.y, currentCell.y);
  const rowTop = state.height - 1 - maxY;
  const x = bounds.offsetX + minX * bounds.cellSize;
  const y = bounds.offsetY + rowTop * bounds.cellSize;
  const width = (maxX - minX + 1) * bounds.cellSize;
  const height = (maxY - minY + 1) * bounds.cellSize;
  ctx.save();
  ctx.fillStyle = 'rgba(0, 184, 169, 0.18)';
  ctx.strokeStyle = 'rgba(0, 184, 169, 0.88)';
  ctx.lineWidth = 2;
  ctx.fillRect(x, y, width, height);
  ctx.strokeRect(x, y, width, height);
  ctx.restore();
}

function drawHover(bounds) {
  const hover = state.hoverCell;
  if (!hover) {
    return;
  }
  const rowTop = state.height - 1 - hover.y;
  const x = bounds.offsetX + hover.x * bounds.cellSize;
  const y = bounds.offsetY + rowTop * bounds.cellSize;
  ctx.save();
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.85)';
  ctx.lineWidth = 1.5;
  ctx.strokeRect(x + 0.5, y + 0.5, bounds.cellSize - 1, bounds.cellSize - 1);
  ctx.restore();
}

function render() {
  if (!state.cells.length) {
    return;
  }

  syncStateFromControls();
  const bounds = getGridBounds();
  ctx.clearRect(0, 0, bounds.viewWidth, bounds.viewHeight);

  ctx.fillStyle = '#091018';
  ctx.fillRect(0, 0, bounds.viewWidth, bounds.viewHeight);

  const inflatedCells = getInflatedCells();
  for (let y = 0; y < state.height; y += 1) {
    for (let x = 0; x < state.width; x += 1) {
      const rowTop = state.height - 1 - y;
      const drawX = bounds.offsetX + x * bounds.cellSize;
      const drawY = bounds.offsetY + rowTop * bounds.cellSize;
      ctx.fillStyle = cellFill(state.cells[y][x]);
      ctx.fillRect(drawX, drawY, bounds.cellSize, bounds.cellSize);
      if (inflatedCells.has(`${x}:${y}`)) {
        ctx.fillStyle = 'rgba(249, 115, 22, 0.28)';
        ctx.fillRect(drawX, drawY, bounds.cellSize, bounds.cellSize);
      }
    }
  }

  if (state.showGrid && bounds.cellSize >= 8) {
    ctx.beginPath();
    ctx.strokeStyle = 'rgba(148, 163, 184, 0.14)';
    ctx.lineWidth = 1;
    for (let x = 0; x <= state.width; x += 1) {
      const lineX = bounds.offsetX + x * bounds.cellSize;
      ctx.moveTo(lineX, bounds.offsetY);
      ctx.lineTo(lineX, bounds.offsetY + bounds.heightPx);
    }
    for (let y = 0; y <= state.height; y += 1) {
      const lineY = bounds.offsetY + y * bounds.cellSize;
      ctx.moveTo(bounds.offsetX, lineY);
      ctx.lineTo(bounds.offsetX + bounds.widthPx, lineY);
    }
    ctx.stroke();
  }

  ctx.strokeStyle = 'rgba(255, 255, 255, 0.22)';
  ctx.lineWidth = 2;
  ctx.strokeRect(bounds.offsetX, bounds.offsetY, bounds.widthPx, bounds.heightPx);

  drawRobotOverlay(bounds);
  drawRectPreview(bounds);
  drawHover(bounds);
}

function updateMetrics() {
  let occupiedCount = 0;
  let unknownCount = 0;
  for (let y = 0; y < state.height; y += 1) {
    for (let x = 0; x < state.width; x += 1) {
      const value = state.cells[y][x];
      if (value >= 65) {
        occupiedCount += 1;
      } else if (value < 0) {
        unknownCount += 1;
      }
    }
  }
  ui.mapSize.textContent = `${state.width} x ${state.height}`;
  ui.mapSpan.textContent = `${(state.width * state.resolution).toFixed(2)}m x ${(state.height * state.resolution).toFixed(2)}m`;
  ui.occupied.textContent = occupiedCount.toString();
  ui.unknown.textContent = unknownCount.toString();
  ui.yamlPreview.value = buildYaml();
}

function updateHoverMetrics(cell) {
  if (!cell) {
    ui.cursor.textContent = '-, -';
    ui.value.textContent = '--';
    return;
  }
  ui.cursor.textContent = `${cell.x}, ${cell.y}`;
  ui.value.textContent = formatCellValue(state.cells[cell.y][cell.x]);
}

function formatCellValue(value) {
  if (value < 0) {
    return 'Unknown (-1)';
  }
  if (value >= 65) {
    return `Occupied (${value})`;
  }
  return `Free (${value})`;
}

function buildCsv() {
  const rows = [];
  for (let srcRow = 0; srcRow < state.height; srcRow += 1) {
    const y = state.row0Bottom ? srcRow : (state.height - 1 - srcRow);
    rows.push(state.cells[y].join(','));
  }
  return rows.join('\n');
}

function yamlStubPath() {
  const withoutExt = state.fileName.replace(/\.csv$/i, '');
  return `/absolute/path/to/${withoutExt}.csv`;
}

function buildYaml() {
  return [
    'csv_map_publisher:',
    '  ros__parameters:',
    `    csv_path: "${yamlStubPath()}"`,
    `    resolution: ${state.resolution.toFixed(3)}`,
    `    frame_id: "${state.frameId}"`,
    `    origin_x: ${state.originX.toFixed(2)}`,
    `    origin_y: ${state.originY.toFixed(2)}`,
    `    origin_yaw: ${state.originYaw.toFixed(2)}`,
    `    row0_bottom: ${state.row0Bottom ? 'true' : 'false'}`,
    '    value_mode: "occupancy"',
    '    occupancy_scale: 1.0',
    '    publish_rate: 0.0',
  ].join('\n');
}

function downloadText(filename, content, mimeType) {
  const blob = new Blob([content], { type: mimeType });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = filename;
  document.body.appendChild(anchor);
  anchor.click();
  anchor.remove();
  URL.revokeObjectURL(url);
}

function loadCsvText(text, sourceName = 'map.csv') {
  const rows = text
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter((line) => line.length > 0)
    .map((line) => line.split(',').map((token) => Number(token.trim())));

  if (!rows.length) {
    throw new Error('CSV is empty.');
  }
  const width = rows[0].length;
  if (!width || rows.some((row) => row.length !== width || row.some((value) => Number.isNaN(value)))) {
    throw new Error('CSV must be a rectangular matrix of numeric values.');
  }

  pushHistory();
  state.width = width;
  state.height = rows.length;
  state.fileName = normalizeCsvFileName(sourceName);
  state.cells = createGrid(state.width, state.height, 0);

  for (let srcRow = 0; srcRow < rows.length; srcRow += 1) {
    const y = state.row0Bottom ? srcRow : (rows.length - 1 - srcRow);
    for (let x = 0; x < width; x += 1) {
      state.cells[y][x] = rows[srcRow][x];
    }
  }

  fitViewToMap();
  syncControlsFromState();
  updateMetrics();
  render();
  setStatus(`Loaded ${sourceName}.`);
}

function clearMap() {
  pushHistory();
  state.cells = createGrid(state.width, state.height, 0);
  updateMetrics();
  render();
  setStatus('Cleared all cells.');
}

function applyResize() {
  const nextWidth = clamp(parseInt(ui.width.value, 10) || state.width, 1, 1024);
  const nextHeight = clamp(parseInt(ui.height.value, 10) || state.height, 1, 1024);
  if (nextWidth === state.width && nextHeight === state.height) {
    setStatus('Map size is unchanged.');
    return;
  }
  pushHistory();
  const nextCells = createGrid(nextWidth, nextHeight, 0);
  for (let y = 0; y < Math.min(state.height, nextHeight); y += 1) {
    for (let x = 0; x < Math.min(state.width, nextWidth); x += 1) {
      nextCells[y][x] = state.cells[y][x];
    }
  }
  state.width = nextWidth;
  state.height = nextHeight;
  state.cells = nextCells;
  fitViewToMap();
  syncControlsFromState();
  updateMetrics();
  render();
  setStatus(`Resized map to ${nextWidth} x ${nextHeight}.`);
}

function loadSample() {
  pushHistory();
  state.width = 48;
  state.height = 32;
  state.resolution = 0.05;
  state.originX = -1.2;
  state.originY = -0.8;
  state.fileName = 'sample_field.csv';
  state.cells = createGrid(state.width, state.height, 0);

  for (let x = 0; x < state.width; x += 1) {
    state.cells[0][x] = 100;
    state.cells[state.height - 1][x] = 100;
  }
  for (let y = 0; y < state.height; y += 1) {
    state.cells[y][0] = 100;
    state.cells[y][state.width - 1] = 100;
  }
  for (let x = 10; x < 36; x += 1) {
    state.cells[12][x] = 100;
  }
  for (let y = 8; y < 22; y += 1) {
    state.cells[y][22] = 100;
  }
  for (let y = 18; y < 24; y += 1) {
    for (let x = 30; x < 36; x += 1) {
      state.cells[y][x] = -1;
    }
  }

  fitViewToMap();
  syncControlsFromState();
  updateMetrics();
  render();
  setStatus('Loaded sample field.');
}

function pickCell(cell) {
  if (!cell) {
    return;
  }
  setDrawValue(state.cells[cell.y][cell.x]);
  setTool('draw');
}

function beginPointerAction(event) {
  const cell = canvasPointToCell(event.clientX, event.clientY);
  state.pointer.isDown = true;
  state.pointer.currentCell = cell;
  state.pointer.startCell = cell;
  state.pointer.lastPaintCell = null;

  if (state.tool === 'pan') {
    state.pointer.mode = 'pan';
    state.pointer.panStartX = event.clientX;
    state.pointer.panStartY = event.clientY;
    state.pointer.startPanX = state.panX;
    state.pointer.startPanY = state.panY;
    canvas.style.cursor = 'grabbing';
    return;
  }

  if (!cell) {
    state.pointer.mode = null;
    return;
  }

  if (state.tool === 'pick') {
    pickCell(cell);
    state.pointer.isDown = false;
    return;
  }

  pushHistory();
  state.pointer.mode = state.tool;
  if (state.tool === 'draw') {
    applyBrushAt(cell);
    state.pointer.lastPaintCell = `${cell.x}:${cell.y}`;
    updateMetrics();
    render();
  } else if (state.tool === 'fill') {
    floodFill(cell);
    state.pointer.isDown = false;
    updateMetrics();
    render();
  }
}

function movePointerAction(event) {
  const cell = canvasPointToCell(event.clientX, event.clientY);
  state.hoverCell = cell;
  state.pointer.currentCell = cell;
  updateHoverMetrics(cell);

  if (!state.pointer.isDown) {
    render();
    return;
  }

  if (state.pointer.mode === 'pan') {
    state.panX = state.pointer.startPanX + (event.clientX - state.pointer.panStartX);
    state.panY = state.pointer.startPanY + (event.clientY - state.pointer.panStartY);
    render();
    return;
  }

  if (state.pointer.mode === 'draw' && cell) {
    const key = `${cell.x}:${cell.y}`;
    if (key !== state.pointer.lastPaintCell) {
      applyBrushAt(cell);
      state.pointer.lastPaintCell = key;
      updateMetrics();
      render();
    }
    return;
  }

  if (state.pointer.mode === 'rect') {
    render();
  }
}

function endPointerAction() {
  if (state.pointer.mode === 'rect' && state.pointer.startCell && state.pointer.currentCell) {
    applyRect(state.pointer.startCell, state.pointer.currentCell);
    updateMetrics();
  }
  state.pointer.isDown = false;
  state.pointer.mode = null;
  canvas.style.cursor = state.tool === 'pan' ? 'grab' : state.tool === 'pick' ? 'copy' : 'crosshair';
  render();
}

function onWheel(event) {
  event.preventDefault();
  const oldZoom = state.zoom;
  const nextZoom = clamp(state.zoom * (event.deltaY > 0 ? 0.92 : 1.08), 0.2, 20.0);
  if (nextZoom === oldZoom) {
    return;
  }

  const rect = canvas.getBoundingClientRect();
  const pointerX = event.clientX - rect.left;
  const pointerY = event.clientY - rect.top;
  const before = getGridBounds();
  const worldX = (pointerX - before.offsetX) / before.cellSize;
  const worldY = (pointerY - before.offsetY) / before.cellSize;
  state.zoom = nextZoom;
  ui.zoomPercent.value = Math.round(state.zoom * 100);
  const after = getGridBounds();
  state.panX += pointerX - (after.offsetX + worldX * after.cellSize);
  state.panY += pointerY - (after.offsetY + worldY * after.cellSize);
  render();
}

function attachListeners() {
  window.addEventListener('resize', resizeCanvas);

  document.getElementById('apply-size-btn').addEventListener('click', applyResize);
  document.getElementById('undo-btn').addEventListener('click', undo);
  document.getElementById('load-sample-btn').addEventListener('click', loadSample);
  document.getElementById('reset-view-btn').addEventListener('click', () => {
    fitViewToMap();
    render();
    setStatus('Reset viewport.');
  });

  document.getElementById('load-csv-btn').addEventListener('click', () => fileInput.click());
  document.getElementById('save-csv-btn').addEventListener('click', () => {
    syncStateFromControls();
    downloadText(state.fileName, buildCsv(), 'text/csv;charset=utf-8');
    setStatus(`Saved ${state.fileName}.`);
  });
  document.getElementById('save-yaml-btn').addEventListener('click', () => {
    syncStateFromControls();
    const yamlName = state.fileName.replace(/\.csv$/i, '.yaml');
    downloadText(yamlName, buildYaml(), 'text/yaml;charset=utf-8');
    setStatus(`Saved ${yamlName}.`);
  });
  document.getElementById('copy-yaml-btn').addEventListener('click', async () => {
    syncStateFromControls();
    try {
      await navigator.clipboard.writeText(buildYaml());
      setStatus('Copied YAML to clipboard.');
    } catch (error) {
      setStatus('Clipboard write failed. Use Save YAML instead.');
    }
  });
  document.getElementById('clear-map-btn').addEventListener('click', clearMap);

  fileInput.addEventListener('change', async (event) => {
    const file = event.target.files && event.target.files[0];
    if (!file) {
      return;
    }
    try {
      const text = await file.text();
      loadCsvText(text, file.name);
    } catch (error) {
      setStatus(`CSV load failed: ${error.message}`);
    } finally {
      fileInput.value = '';
    }
  });

  document.querySelectorAll('[data-tool]').forEach((button) => {
    button.addEventListener('click', () => setTool(button.dataset.tool));
  });
  document.querySelectorAll('[data-value]').forEach((button) => {
    button.addEventListener('click', () => setDrawValue(Number(button.dataset.value)));
  });

  [
    ui.resolution,
    ui.frameId,
    ui.originX,
    ui.originY,
    ui.originYaw,
    ui.fileName,
    ui.row0Bottom,
    ui.brushSize,
    ui.zoomPercent,
    ui.robotRadius,
    ui.inflationRadius,
    ui.showGrid,
    ui.showInflation,
  ].forEach((element) => {
    element.addEventListener('change', () => {
      syncStateFromControls();
      updateMetrics();
      render();
    });
  });

  canvas.addEventListener('pointerdown', (event) => {
    canvas.setPointerCapture(event.pointerId);
    beginPointerAction(event);
  });
  canvas.addEventListener('pointermove', movePointerAction);
  canvas.addEventListener('pointerup', endPointerAction);
  canvas.addEventListener('pointerleave', () => {
    state.hoverCell = null;
    updateHoverMetrics(null);
    if (!state.pointer.isDown) {
      render();
    }
  });
  canvas.addEventListener('wheel', onWheel, { passive: false });
  canvas.addEventListener('contextmenu', (event) => event.preventDefault());

  window.addEventListener('keydown', (event) => {
    const tag = document.activeElement ? document.activeElement.tagName : '';
    const isInput = tag === 'INPUT' || tag === 'TEXTAREA';
    if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'z') {
      event.preventDefault();
      undo();
      return;
    }
    if (isInput) {
      return;
    }
    if (event.key === '1') setTool('draw');
    if (event.key === '2') setTool('rect');
    if (event.key === '3') setTool('fill');
    if (event.key === '4') setTool('pick');
    if (event.key === ' ') setTool('pan');
  });
}

function initialize() {
  state.cells = createGrid(state.width, state.height, 0);
  syncControlsFromState();
  setTool('draw');
  setDrawValue(100);
  fitViewToMap();
  updateMetrics();
  attachListeners();
  resizeCanvas();
  setStatus('Draw cells, then export CSV and YAML.');
}

initialize();
