interface ColorScaleBarProps {
  min: number;
  max: number;
  colormap?: string;
}

function ColorScaleBar({ min, max, colormap = 'turbo' }: ColorScaleBarProps) {
  return (
    <div className="color-scale-bar">
      <span className="scale-label">{max.toFixed(2)}</span>
      <div
        style={{
          width: 16,
          height: 200,
          background: `linear-gradient(to bottom, #ff0000, #ffff00, #00ff00, #00ffff, #0000ff)`,
          borderRadius: 2,
          border: '1px solid var(--border)',
        }}
      />
      <span className="scale-label">{min.toFixed(2)}</span>
      <span className="scale-label" style={{ fontSize: 9, color: 'var(--text-muted)' }}>
        {colormap}
      </span>
    </div>
  );
}

export default ColorScaleBar;
