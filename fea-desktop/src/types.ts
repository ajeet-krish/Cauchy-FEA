export interface Point {
  x: number;
  y: number;
}

export interface Shape {
  id: string;
  type: 'rectangle' | 'circle' | 'polygon' | 'ibeam' | 'lbracket';
  name: string;
  x: number;
  y: number;
  radius?: number;
  width?: number;
  height?: number;
  points?: Point[];
  flange?: number;
  web?: number;
}

export interface DirichletBC {
  node: number;
  dof: number;
  value: number;
}

export interface NeumannBC {
  node: number;
  dof: number;
  value: number;
}

export interface Material {
  E: number;
  nu: number;
  rho: number;
  t: number;
  alpha?: number;
}

export interface MeshData {
  nodes: Point[];
  elements: { type: string; nodes: number[] }[];
  dirichlet: DirichletBC[];
  neumann: NeumannBC[];
  material: Material;
  plane: string;
  num_nodes: number;
  num_elements: number;
  num_dofs: number;
}

export interface SolveResult {
  displacements: { ux: number; uy: number }[];
  stresses: {
    sigma_xx: number;
    sigma_yy: number;
    sigma_xy: number;
    von_mises: number;
    sigma_1: number;
    sigma_2: number;
  }[];
  max_displacement: number;
  max_stress: number;
  solve_time_ms: number;
  cg_iterations: number;
  cg_converged: boolean;
  num_nodes: number;
  num_elements: number;
}

export interface ProjectState {
  shapes: Shape[];
  mesh: MeshData | null;
  dirichlet: DirichletBC[];
  neumann: NeumannBC[];
  material: Material;
  planeType: 'stress' | 'strain';
  result: SolveResult | null;
  nx: number;
  ny: number;
  elemType: number; // 0=Q4, 1=Q8, 2=T3
}
