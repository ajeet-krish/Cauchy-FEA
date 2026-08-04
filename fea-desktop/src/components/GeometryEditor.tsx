import type { Shape } from '../types';

interface GeometryEditorProps {
  shapes: Shape[];
  onChange: (shapes: Shape[]) => void;
}

function GeometryEditor({ shapes, onChange }: GeometryEditorProps) {
  const addShape = (type: Shape['type']) => {
    const id = `shape-${Date.now()}`;
    const name = `${type.charAt(0).toUpperCase() + type.slice(1)} ${shapes.length + 1}`;
    const newShape: Shape = {
      id,
      type,
      name,
      x: 0,
      y: 0,
      ...(type === 'circle' ? { radius: 0.5 } : { width: 1.0, height: 0.5 }),
    };
    onChange([...shapes, newShape]);
  };

  const removeShape = (id: string) => {
    onChange(shapes.filter((s) => s.id !== id));
  };

  return (
    <div className="geometry-editor">
      <div className="editor-toolbar">
        <div className="tool-group">
          <button className="tool-btn" onClick={() => addShape('rectangle')}>
            Rect
          </button>
          <button className="tool-btn" onClick={() => addShape('circle')}>
            Circle
          </button>
          <button className="tool-btn" onClick={() => addShape('polygon')}>
            Polygon
          </button>
        </div>
      </div>

      {shapes.length > 0 ? (
        <div className="shape-list">
          {shapes.map((shape) => (
            <div key={shape.id} className="shape-item">
              <span className="shape-info">
                {shape.name} ({shape.type})
              </span>
              <button
                className="shape-delete"
                onClick={() => removeShape(shape.id)}
                aria-label={`Remove ${shape.name}`}
              >
                &#10005;
              </button>
            </div>
          ))}
        </div>
      ) : (
        <div className="placeholder">
          Draw shapes to define your structural domain
        </div>
      )}
    </div>
  );
}

export default GeometryEditor;
