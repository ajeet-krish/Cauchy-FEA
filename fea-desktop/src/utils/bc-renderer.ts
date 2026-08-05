import type { DirichletBC, NeumannBC, Point } from '../types';

const YELLOW = '#ffb347';
const YELLOW_DIM = 'rgba(255, 179, 71, 0.5)';
const GREEN_ARROW = '#3fb950';
const RED_ARROW = '#f85149';

/**
 * Draw a fixed support triangle at a node position.
 * Triangle points downward (standard FEA notation).
 */
function drawFixedTriangle(
  ctx: CanvasRenderingContext2D,
  sx: number,
  sy: number,
  size: number,
): void {
  const h = size * 0.9;
  const w = size * 0.7;

  // Main triangle
  ctx.fillStyle = YELLOW;
  ctx.strokeStyle = YELLOW_DIM;
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.moveTo(sx, sy);
  ctx.lineTo(sx - w / 2, sy - h);
  ctx.lineTo(sx + w / 2, sy - h);
  ctx.closePath();
  ctx.fill();
  ctx.stroke();

  // Ground hash lines
  ctx.strokeStyle = YELLOW_DIM;
  ctx.lineWidth = 1;
  const groundY = sy - h - 1;
  const hashLen = w * 0.6;
  for (let i = -1; i <= 1; i++) {
    const hx = sx + i * (w / 3);
    ctx.beginPath();
    ctx.moveTo(hx, groundY);
    ctx.lineTo(hx - hashLen * 0.4, groundY - 4);
    ctx.stroke();
  }
}

/**
 * Draw a fixed UX constraint (triangle + horizontal bar).
 */
export function drawFixedUX(
  ctx: CanvasRenderingContext2D,
  sx: number,
  sy: number,
  size: number,
): void {
  drawFixedTriangle(ctx, sx, sy, size);

  // Horizontal line through node (X constraint indicator)
  ctx.strokeStyle = YELLOW;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(sx - size * 0.5, sy);
  ctx.lineTo(sx + size * 0.5, sy);
  ctx.stroke();
}

/**
 * Draw a fixed UY constraint (triangle + vertical bar).
 */
export function drawFixedUY(
  ctx: CanvasRenderingContext2D,
  sx: number,
  sy: number,
  size: number,
): void {
  drawFixedTriangle(ctx, sx, sy, size);

  // Vertical line through node (Y constraint indicator)
  ctx.strokeStyle = YELLOW;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(sx, sy - size * 0.2);
  ctx.lineTo(sx, sy + size * 0.4);
  ctx.stroke();
}

/**
 * Draw a roller support (circle at the node).
 */
function drawRoller(
  ctx: CanvasRenderingContext2D,
  sx: number,
  sy: number,
  size: number,
): void {
  const r = size * 0.35;

  // Circle
  ctx.fillStyle = YELLOW;
  ctx.strokeStyle = YELLOW_DIM;
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.arc(sx, sy + r * 0.3, r, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();

  // Ground line
  ctx.strokeStyle = YELLOW_DIM;
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(sx - r * 1.5, sy + r * 1.2);
  ctx.lineTo(sx + r * 1.5, sy + r * 1.2);
  ctx.stroke();
}

/**
 * Draw a force arrow in the X direction.
 */
function drawForceX(
  ctx: CanvasRenderingContext2D,
  sx: number,
  sy: number,
  size: number,
  positive: boolean,
): void {
  const len = size * 1.5;
  const dx = positive ? len : -len;
  const color = positive ? GREEN_ARROW : RED_ARROW;

  ctx.strokeStyle = color;
  ctx.fillStyle = color;
  ctx.lineWidth = 2.5;

  // Shaft
  ctx.beginPath();
  ctx.moveTo(sx, sy);
  ctx.lineTo(sx + dx, sy);
  ctx.stroke();

  // Arrowhead
  const headSize = size * 0.35;
  const dir = positive ? 1 : -1;
  ctx.beginPath();
  ctx.moveTo(sx + dx, sy);
  ctx.lineTo(sx + dx - dir * headSize, sy - headSize * 0.6);
  ctx.lineTo(sx + dx - dir * headSize, sy + headSize * 0.6);
  ctx.closePath();
  ctx.fill();
}

/**
 * Draw a force arrow in the Y direction.
 */
function drawForceY(
  ctx: CanvasRenderingContext2D,
  sx: number,
  sy: number,
  size: number,
  positive: boolean,
): void {
  const len = size * 1.5;
  const dy = positive ? -len : len;
  const color = positive ? GREEN_ARROW : RED_ARROW;

  ctx.strokeStyle = color;
  ctx.fillStyle = color;
  ctx.lineWidth = 2.5;

  // Shaft
  ctx.beginPath();
  ctx.moveTo(sx, sy);
  ctx.lineTo(sx, sy + dy);
  ctx.stroke();

  // Arrowhead
  const headSize = size * 0.35;
  const dir = positive ? -1 : 1;
  ctx.beginPath();
  ctx.moveTo(sx, sy + dy);
  ctx.lineTo(sx - headSize * 0.6, sy + dy - dir * headSize);
  ctx.lineTo(sx + headSize * 0.6, sy + dy - dir * headSize);
  ctx.closePath();
  ctx.fill();
}

/**
 * Find the unique boundary conditions for a node, grouping DOFs.
 * Returns a summary string for tooltip display.
 */
export function getNodeBCSummary(
  dirichlet: DirichletBC[],
  neumann: NeumannBC[],
  nodeIndex: number,
): string | null {
  const parts: string[] = [];
  const dirichletForNode = dirichlet.filter((bc) => bc.node === nodeIndex);
  const neumannForNode = neumann.filter((bc) => bc.node === nodeIndex);

  if (dirichletForNode.length > 0) {
    const dofs = dirichletForNode.map((bc) => (bc.dof === 0 ? 'UX' : 'UY'));
    parts.push(`Fixed ${dofs.join('+')}`);
  }
  if (neumannForNode.length > 0) {
    for (const bc of neumannForNode) {
      const dir = bc.dof === 0 ? 'FX' : 'FY';
      parts.push(`${dir}=${bc.value} N`);
    }
  }

  return parts.length > 0 ? parts.join(', ') : null;
}

/**
 * Render boundary condition symbols onto a 2D canvas context.
 * Triangles for fixed supports, circles for rollers, arrows for forces.
 *
 * @param ctx Canvas 2D rendering context
 * @param dirichlet Array of Dirichlet boundary conditions
 * @param neumann Array of Neumann boundary conditions
 * @param nodes Array of mesh node positions (world coordinates)
 * @param toScreen Function to convert world coordinates to screen coordinates
 * @param symbolSize Base size for BC symbols in pixels
 */
export function renderBCs(
  ctx: CanvasRenderingContext2D,
  dirichlet: DirichletBC[],
  neumann: NeumannBC[],
  nodes: Point[],
  toScreen: (pt: Point) => { sx: number; sy: number },
  symbolSize: number = 16,
): void {
  // Pass 1: Classify each Dirichlet node as fixed (UX+UY) or roller (single DOF)
  const fixedNodes = new Set<number>();
  const rollerNodes = new Set<number>();
  const nodeDofMap = new Map<number, Set<number>>();

  for (const bc of dirichlet) {
    if (bc.node < 0 || bc.node >= nodes.length) continue;
    const dofs = nodeDofMap.get(bc.node) ?? new Set<number>();
    dofs.add(bc.dof);
    nodeDofMap.set(bc.node, dofs);
  }

  for (const [nodeIdx, dofs] of nodeDofMap) {
    if (dofs.has(0) && dofs.has(1)) {
      fixedNodes.add(nodeIdx);
    } else {
      rollerNodes.add(nodeIdx);
    }
  }

  // Pass 2: Draw fixed support symbols (triangles)
  for (const nodeIdx of fixedNodes) {
    const { sx, sy } = toScreen(nodes[nodeIdx]);
    drawFixedTriangle(ctx, sx, sy, symbolSize);
  }

  // Pass 3: Draw roller support symbols (circles) -- only for nodes NOT in fixed set
  for (const nodeIdx of rollerNodes) {
    if (fixedNodes.has(nodeIdx)) continue;
    const { sx, sy } = toScreen(nodes[nodeIdx]);
    drawRoller(ctx, sx, sy, symbolSize);
  }

  // Pass 4: Draw Neumann BCs (force arrows)
  for (const bc of neumann) {
    if (bc.node < 0 || bc.node >= nodes.length) continue;
    const { sx, sy } = toScreen(nodes[bc.node]);
    const positive = bc.value >= 0;

    if (bc.dof === 0) {
      drawForceX(ctx, sx, sy, symbolSize, positive);
    } else {
      drawForceY(ctx, sx, sy, symbolSize, positive);
    }
  }
}

/**
 * Draw a highlight ring around a selected node.
 */
export function drawSelectedNode(
  ctx: CanvasRenderingContext2D,
  sx: number,
  sy: number,
  radius: number = 8,
): void {
  ctx.strokeStyle = '#00d4ff';
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.arc(sx, sy, radius, 0, Math.PI * 2);
  ctx.stroke();

  // Inner glow
  ctx.strokeStyle = 'rgba(0, 212, 255, 0.3)';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.arc(sx, sy, radius + 3, 0, Math.PI * 2);
  ctx.stroke();
}

/**
 * Draw a hover highlight around the nearest node when a BC tool is active.
 */
export function drawHoverNode(
  ctx: CanvasRenderingContext2D,
  sx: number,
  sy: number,
  radius: number = 6,
): void {
  ctx.strokeStyle = 'rgba(255, 179, 71, 0.6)';
  ctx.lineWidth = 1.5;
  ctx.setLineDash([3, 3]);
  ctx.beginPath();
  ctx.arc(sx, sy, radius, 0, Math.PI * 2);
  ctx.stroke();
  ctx.setLineDash([]);
}
