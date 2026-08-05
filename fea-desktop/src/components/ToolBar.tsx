interface ToolBarProps {
  onNew: () => void;
  onOpen: () => void;
  onSave: () => void;
  onExportPng: () => void;
  isDirty?: boolean;
  hasResults?: boolean;
}

function ToolBar({ onNew, onOpen, onSave, onExportPng, isDirty, hasResults }: ToolBarProps) {
  return (
    <div className="toolbar-buttons">
      <button className="toolbar-btn" onClick={onNew} aria-label="New project">
        New
      </button>
      <button className="toolbar-btn" onClick={onOpen} aria-label="Open project">
        Open
      </button>
      <button className="toolbar-btn" onClick={onSave} aria-label="Save project">
        Save{isDirty ? '*' : ''}
      </button>
      <div className="toolbar-separator" />
      <button
        className="toolbar-btn"
        onClick={onExportPng}
        aria-label="Export PNG"
        disabled={!hasResults}
      >
        Export PNG
      </button>
    </div>
  );
}

export default ToolBar;
