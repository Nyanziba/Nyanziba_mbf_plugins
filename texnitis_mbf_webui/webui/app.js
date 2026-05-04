// ===== texnitis_move_base_like Debug Dashboard =====

const ros = new ROSLIB.Ros();
const statusBadge = document.getElementById('connection-status');
const statusText = statusBadge.querySelector('.text');

function connect() {
  ros.connect('ws://localhost:9090');
}

ros.on('connection', () => {
  statusBadge.className = 'status-badge connected';
  statusText.textContent = 'Connected';
});
ros.on('error', () => {
  statusBadge.className = 'status-badge disconnected';
  statusText.textContent = 'Error';
});
ros.on('close', () => {
  statusBadge.className = 'status-badge disconnected';
  statusText.textContent = 'Disconnected';
  setTimeout(connect, 2000);
});
connect();

// ==================== Helpers ====================

function eulerFromQuaternion(q) {
  const siny_cosp = 2 * (q.w * q.z + q.x * q.y);
  const cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
  return Math.atan2(siny_cosp, cosy_cosp);
}

// ==================== State ====================

let robotPose = { x: 0, y: 0, yaw: 0 };
let goalPose = null;    // {x, y, yaw} or null
let globalPath = [];    // [{x,y}, ...]
let localPath = [];     // [{x,y}, ...]
let navState = {};

// Canvas view state
let followRobot = true;
let viewCenterX = 0;
let viewCenterY = 0;
let viewScale = 80;  // pixels per meter

// ==================== Topic Subscriptions ====================

// 1. Fused Pose
const fusedPoseListener = new ROSLIB.Topic({
  ros, name: '/move_base_like_node/fused_pose',
  messageType: 'geometry_msgs/msg/PoseStamped'
});
fusedPoseListener.subscribe((msg) => {
  const p = msg.pose.position;
  const q = msg.pose.orientation;
  const yaw = eulerFromQuaternion(q);
  robotPose = { x: p.x, y: p.y, yaw };
  document.getElementById('pose-x').innerText = p.x.toFixed(3);
  document.getElementById('pose-y').innerText = p.y.toFixed(3);
  document.getElementById('pose-yaw').innerText = yaw.toFixed(3);
  updateDistance();
});

// 2. Current Goal
const goalListener = new ROSLIB.Topic({
  ros, name: '/move_base_like_node/current_goal',
  messageType: 'geometry_msgs/msg/PoseStamped'
});
goalListener.subscribe((msg) => {
  const p = msg.pose.position;
  const q = msg.pose.orientation;
  const yaw = eulerFromQuaternion(q);
  goalPose = { x: p.x, y: p.y, yaw };
  document.getElementById('goal-x').innerText = p.x.toFixed(3);
  document.getElementById('goal-y').innerText = p.y.toFixed(3);
  document.getElementById('goal-yaw').innerText = yaw.toFixed(3);
  updateDistance();
});

// 3. Global Path
const globalPathListener = new ROSLIB.Topic({
  ros, name: '/global_path',
  messageType: 'nav_msgs/msg/Path'
});
globalPathListener.subscribe((msg) => {
  globalPath = msg.poses.map(ps => ({
    x: ps.pose.position.x,
    y: ps.pose.position.y
  }));
});

// 4. Local Path (lookahead)
const localPathListener = new ROSLIB.Topic({
  ros, name: '/local_path',
  messageType: 'nav_msgs/msg/Path'
});
localPathListener.subscribe((msg) => {
  localPath = msg.poses.map(ps => ({
    x: ps.pose.position.x,
    y: ps.pose.position.y
  }));
});

// 5. Navigation State (JSON)
const navStateListener = new ROSLIB.Topic({
  ros, name: '/move_base_like_node/nav_state',
  messageType: 'std_msgs/msg/String'
});
navStateListener.subscribe((msg) => {
  try {
    navState = JSON.parse(msg.data);
    updateNavStateUI(navState);
  } catch (e) { /* ignore parse errors */ }
});

// 6. Command Velocity
const cmdVelListener = new ROSLIB.Topic({
  ros, name: '/cmd_vel',
  messageType: 'geometry_msgs/msg/Twist'
});
cmdVelListener.subscribe((msg) => {
  const vx = msg.linear.x;
  const vy = msg.linear.y;
  const wz = msg.angular.z;
  document.getElementById('vel-x').innerText = vx.toFixed(3);
  document.getElementById('vel-y').innerText = vy.toFixed(3);
  document.getElementById('vel-yaw').innerText = wz.toFixed(3);

  // Velocity bar visualization
  const barX = document.getElementById('vel-bar-x');
  if (vx >= 0) {
    barX.style.width = Math.min((vx / 1.0) * 50, 50) + '%';
    barX.style.marginLeft = '50%';
  } else {
    const w = Math.min((Math.abs(vx) / 1.0) * 50, 50);
    barX.style.width = w + '%';
    barX.style.marginLeft = (50 - w) + '%';
  }
  const barYaw = document.getElementById('vel-bar-yaw');
  if (wz >= 0) {
    barYaw.style.width = Math.min((wz / 2.0) * 50, 50) + '%';
    barYaw.style.marginLeft = '50%';
  } else {
    const w = Math.min((Math.abs(wz) / 2.0) * 50, 50);
    barYaw.style.width = w + '%';
    barYaw.style.marginLeft = (50 - w) + '%';
  }
});

// ==================== Distance Helper ====================

function updateDistance() {
  if (goalPose) {
    const dx = goalPose.x - robotPose.x;
    const dy = goalPose.y - robotPose.y;
    const dist = Math.sqrt(dx * dx + dy * dy);
    document.getElementById('distance-val').innerText = dist.toFixed(3) + 'm';
  } else {
    document.getElementById('distance-val').innerText = '--';
  }
}

// ==================== Nav State UI ====================

function updateNavStateUI(s) {
  const setEl = (id, val) => {
    const el = document.getElementById(id);
    if (el) el.innerText = val;
  };

  setEl('state-pose-source', s.pose_source || '--');
  setEl('state-has-goal', s.has_goal ? '✅ Yes' : '❌ No');
  setEl('state-has-path', s.has_path ? '✅ Yes' : '❌ No');
  setEl('state-path-length', s.path_length !== undefined ? s.path_length + ' pts' : '--');
  setEl('state-distance', s.distance_to_goal !== undefined ? s.distance_to_goal.toFixed(3) + 'm' : '--');
  setEl('state-yaw-diff', s.yaw_diff_to_goal !== undefined ? s.yaw_diff_to_goal.toFixed(3) + ' rad' : '--');
  setEl('state-xy-reached', s.xy_reached !== undefined ? (s.xy_reached ? '✅ YES' : '❌ No') : '--');
  setEl('state-yaw-reached', s.yaw_reached !== undefined ? (s.yaw_reached ? '✅ YES' : '❌ No') : '--');
  setEl('state-last-error', s.last_error || '--');

  // Color-code reached indicators
  const xyEl = document.getElementById('state-xy-reached');
  const yawEl = document.getElementById('state-yaw-reached');
  if (xyEl) xyEl.className = 'state-value ' + (s.xy_reached ? 'reached' : 'not-reached');
  if (yawEl) yawEl.className = 'state-value ' + (s.yaw_reached ? 'reached' : 'not-reached');

  // Pose source badge color
  const srcEl = document.getElementById('state-pose-source');
  if (srcEl) {
    srcEl.className = 'state-value badge badge-' + (s.pose_source || 'none');
  }
}

// ==================== 2D Canvas Visualization ====================

const canvas = document.getElementById('nav-canvas');
const ctx = canvas.getContext('2d');

// High-DPI support
function resizeCanvas() {
  const wrapper = canvas.parentElement;
  const rect = wrapper.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  canvas.width = rect.width * dpr;
  canvas.height = rect.height * dpr;
  canvas.style.width = rect.width + 'px';
  canvas.style.height = rect.height + 'px';
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}

window.addEventListener('resize', resizeCanvas);
resizeCanvas();

// Pan & zoom controls
let isDragging = false;
let dragStartX = 0, dragStartY = 0;
let dragViewStartX = 0, dragViewStartY = 0;

canvas.addEventListener('mousedown', (e) => {
  isDragging = true;
  followRobot = false;
  document.getElementById('btn-follow-robot').classList.remove('active');
  dragStartX = e.clientX;
  dragStartY = e.clientY;
  dragViewStartX = viewCenterX;
  dragViewStartY = viewCenterY;
  canvas.style.cursor = 'grabbing';
});

canvas.addEventListener('mousemove', (e) => {
  if (!isDragging) return;
  const dx = (e.clientX - dragStartX) / viewScale;
  const dy = (e.clientY - dragStartY) / viewScale;
  viewCenterX = dragViewStartX - dx;
  viewCenterY = dragViewStartY + dy;  // Y is flipped
});

canvas.addEventListener('mouseup', () => {
  isDragging = false;
  canvas.style.cursor = 'grab';
});

canvas.addEventListener('mouseleave', () => {
  isDragging = false;
  canvas.style.cursor = 'grab';
});

canvas.addEventListener('wheel', (e) => {
  e.preventDefault();
  const zoomFactor = e.deltaY > 0 ? 0.9 : 1.1;
  viewScale = Math.max(5, Math.min(500, viewScale * zoomFactor));
}, { passive: false });

// UI buttons
document.getElementById('btn-reset-view').addEventListener('click', () => {
  viewScale = 80;
  followRobot = true;
  document.getElementById('btn-follow-robot').classList.add('active');
});

document.getElementById('btn-follow-robot').addEventListener('click', () => {
  followRobot = !followRobot;
  document.getElementById('btn-follow-robot').classList.toggle('active', followRobot);
});

// ---- Drawing Functions ----

function worldToCanvas(wx, wy) {
  const cw = canvas.width / (window.devicePixelRatio || 1);
  const ch = canvas.height / (window.devicePixelRatio || 1);
  const cx = cw / 2 + (wx - viewCenterX) * viewScale;
  const cy = ch / 2 - (wy - viewCenterY) * viewScale;  // Y flip
  return [cx, cy];
}

function drawGrid() {
  const cw = canvas.width / (window.devicePixelRatio || 1);
  const ch = canvas.height / (window.devicePixelRatio || 1);

  // Determine grid spacing (in meters)
  let gridSize = 1.0;
  if (viewScale < 20) gridSize = 5.0;
  else if (viewScale < 50) gridSize = 2.0;
  else if (viewScale > 200) gridSize = 0.5;

  const minWx = viewCenterX - cw / (2 * viewScale);
  const maxWx = viewCenterX + cw / (2 * viewScale);
  const minWy = viewCenterY - ch / (2 * viewScale);
  const maxWy = viewCenterY + ch / (2 * viewScale);

  ctx.strokeStyle = 'rgba(255,255,255,0.06)';
  ctx.lineWidth = 0.5;

  // Vertical grid lines
  const startX = Math.floor(minWx / gridSize) * gridSize;
  for (let x = startX; x <= maxWx; x += gridSize) {
    const [cx] = worldToCanvas(x, 0);
    ctx.beginPath();
    ctx.moveTo(cx, 0);
    ctx.lineTo(cx, ch);
    ctx.stroke();
  }

  // Horizontal grid lines
  const startY = Math.floor(minWy / gridSize) * gridSize;
  for (let y = startY; y <= maxWy; y += gridSize) {
    const [, cy] = worldToCanvas(0, y);
    ctx.beginPath();
    ctx.moveTo(0, cy);
    ctx.lineTo(cw, cy);
    ctx.stroke();
  }

  // Draw axes through origin with slightly more visibility
  ctx.strokeStyle = 'rgba(255,255,255,0.15)';
  ctx.lineWidth = 1;
  const [ox, oy] = worldToCanvas(0, 0);
  // X axis
  ctx.beginPath(); ctx.moveTo(0, oy); ctx.lineTo(cw, oy); ctx.stroke();
  // Y axis
  ctx.beginPath(); ctx.moveTo(ox, 0); ctx.lineTo(ox, ch); ctx.stroke();

  // Scale indicator
  ctx.fillStyle = 'rgba(255,255,255,0.3)';
  ctx.font = '11px Outfit, sans-serif';
  ctx.fillText(`${gridSize}m grid | ${viewScale.toFixed(0)} px/m`, 10, ch - 10);
}

function drawPath(points, color, lineW, dashed) {
  if (points.length < 2) return;
  ctx.strokeStyle = color;
  ctx.lineWidth = lineW;
  ctx.lineJoin = 'round';
  ctx.lineCap = 'round';
  if (dashed) ctx.setLineDash([6, 4]);
  else ctx.setLineDash([]);

  ctx.beginPath();
  const [sx, sy] = worldToCanvas(points[0].x, points[0].y);
  ctx.moveTo(sx, sy);
  for (let i = 1; i < points.length; i++) {
    const [px, py] = worldToCanvas(points[i].x, points[i].y);
    ctx.lineTo(px, py);
  }
  ctx.stroke();
  ctx.setLineDash([]);
}

function drawArrow(wx, wy, yaw, color, size, label) {
  const [cx, cy] = worldToCanvas(wx, wy);
  const arrowLen = size;
  const arrowW = size * 0.45;

  ctx.save();
  ctx.translate(cx, cy);
  ctx.rotate(-yaw);  // canvas Y is flipped

  // Arrow body
  ctx.fillStyle = color;
  ctx.globalAlpha = 0.9;
  ctx.beginPath();
  ctx.moveTo(arrowLen, 0);
  ctx.lineTo(-arrowLen * 0.3, -arrowW);
  ctx.lineTo(-arrowLen * 0.1, 0);
  ctx.lineTo(-arrowLen * 0.3, arrowW);
  ctx.closePath();
  ctx.fill();

  // Arrow border
  ctx.strokeStyle = 'rgba(0,0,0,0.4)';
  ctx.lineWidth = 1;
  ctx.stroke();

  ctx.globalAlpha = 1.0;
  ctx.restore();

  // Label
  if (label) {
    ctx.fillStyle = color;
    ctx.globalAlpha = 0.8;
    ctx.font = 'bold 11px Outfit, sans-serif';
    ctx.fillText(label, cx + size + 4, cy - size - 2);
    ctx.globalAlpha = 1.0;
  }
}

function drawGoalTolerance(wx, wy, tolerance) {
  const [cx, cy] = worldToCanvas(wx, wy);
  const r = tolerance * viewScale;
  ctx.strokeStyle = 'rgba(255, 71, 87, 0.3)';
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 3]);
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.stroke();
  ctx.setLineDash([]);
}

function drawLookaheadPoint() {
  if (localPath.length < 2) return;
  const lp = localPath[localPath.length - 1];
  const [cx, cy] = worldToCanvas(lp.x, lp.y);
  ctx.fillStyle = '#ffa502';
  ctx.globalAlpha = 0.8;
  ctx.beginPath();
  ctx.arc(cx, cy, 5, 0, Math.PI * 2);
  ctx.fill();
  ctx.globalAlpha = 1.0;
}

// ---- Main Render Loop ----

function render() {
  const cw = canvas.width / (window.devicePixelRatio || 1);
  const ch = canvas.height / (window.devicePixelRatio || 1);

  // Follow robot
  if (followRobot) {
    viewCenterX = robotPose.x;
    viewCenterY = robotPose.y;
  }

  // Clear
  ctx.clearRect(0, 0, cw, ch);

  // Background
  ctx.fillStyle = '#0a0c12';
  ctx.fillRect(0, 0, cw, ch);

  // Grid
  drawGrid();

  // Global path
  drawPath(globalPath, 'rgba(18, 216, 164, 0.6)', 2, false);

  // Local path (lookahead)
  drawPath(localPath, 'rgba(255, 165, 2, 0.8)', 2.5, true);

  // Lookahead point
  drawLookaheadPoint();

  // Goal tolerance circle
  if (goalPose && navState.distance_to_goal !== undefined) {
    drawGoalTolerance(goalPose.x, goalPose.y, 0.5);
  }

  // Goal arrow
  if (goalPose) {
    drawArrow(goalPose.x, goalPose.y, goalPose.yaw, '#ff4757', 14, 'Goal');
  }

  // Robot arrow
  drawArrow(robotPose.x, robotPose.y, robotPose.yaw, '#2e86fb', 16, 'Robot');

  // Coordinate display in top-right
  ctx.fillStyle = 'rgba(255,255,255,0.4)';
  ctx.font = '10px monospace';
  ctx.fillText(`Robot: (${robotPose.x.toFixed(2)}, ${robotPose.y.toFixed(2)})`, cw - 180, 18);
  if (goalPose) {
    ctx.fillText(`Goal:  (${goalPose.x.toFixed(2)}, ${goalPose.y.toFixed(2)})`, cw - 180, 32);
  }

  requestAnimationFrame(render);
}

requestAnimationFrame(render);
