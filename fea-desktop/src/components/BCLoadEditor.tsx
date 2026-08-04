import type { DirichletBC, NeumannBC } from '../types';

interface BCLoadEditorProps {
  dirichlet: DirichletBC[];
  neumann: NeumannBC[];
  onDirichletChange: (bc: DirichletBC[]) => void;
  onNeumannChange: (bc: NeumannBC[]) => void;
}

function BCLoadEditor({
  dirichlet,
  neumann,
  onDirichletChange,
  onNeumannChange,
}: BCLoadEditorProps) {
  const addDirichlet = () => {
    onDirichletChange([
      ...dirichlet,
      { node: 0, dof: 0, value: 0 },
    ]);
  };

  const addNeumann = () => {
    onNeumannChange([
      ...neumann,
      { node: 0, dof: 1, value: -1000 },
    ]);
  };

  const removeDirichlet = (index: number) => {
    onDirichletChange(dirichlet.filter((_, i) => i !== index));
  };

  const removeNeumann = (index: number) => {
    onNeumannChange(neumann.filter((_, i) => i !== index));
  };

  return (
    <div>
      <div className="form-group">
        <label>Fixed Constraints</label>
        {dirichlet.length > 0 ? (
          dirichlet.map((bc, i) => (
            <div key={i} className="shape-item">
              <span className="shape-info">
                Node {bc.node}, DOF {bc.dof === 0 ? 'X' : 'Y'} = {bc.value}
              </span>
              <button
                className="shape-delete"
                onClick={() => removeDirichlet(i)}
                aria-label={`Remove constraint ${i + 1}`}
              >
                &#10005;
              </button>
            </div>
          ))
        ) : (
          <div className="placeholder">No fixed constraints</div>
        )}
        <button className="btn-secondary" onClick={addDirichlet} style={{ marginTop: 6 }}>
          + Add Fixed
        </button>
      </div>

      <div className="form-group" style={{ marginTop: 12 }}>
        <label>Applied Loads</label>
        {neumann.length > 0 ? (
          neumann.map((bc, i) => (
            <div key={i} className="shape-item">
              <span className="shape-info">
                Node {bc.node}, DOF {bc.dof === 0 ? 'X' : 'Y'} = {bc.value} N
              </span>
              <button
                className="shape-delete"
                onClick={() => removeNeumann(i)}
                aria-label={`Remove load ${i + 1}`}
              >
                &#10005;
              </button>
            </div>
          ))
        ) : (
          <div className="placeholder">No applied loads</div>
        )}
        <button className="btn-secondary" onClick={addNeumann} style={{ marginTop: 6 }}>
          + Add Load
        </button>
      </div>
    </div>
  );
}

export default BCLoadEditor;
