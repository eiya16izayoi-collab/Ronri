import React, { useState, useEffect, useRef, useCallback } from 'react';
import { 
  Plus, 
  Trash2, 
  Zap, 
  Play, 
  MousePointer2, 
  Move,
  Info
} from 'lucide-react';

// --- Constants & Types ---
const GATE_TYPES = {
  AND: { inputs: 2, label: 'AND' },
  OR: { inputs: 2, label: 'OR' },
  NOT: { inputs: 1, label: 'NOT' },
  NAND: { inputs: 2, label: 'NAND' },
  NOR: { inputs: 2, label: 'NOR' },
  XOR: { inputs: 2, label: 'XOR' },
  INPUT: { inputs: 0, label: 'IN' },
  OUTPUT: { inputs: 1, label: 'OUT' },
};

const GRID_SIZE = 20;

const App = () => {
  const [components, setComponents] = useState([]);
  const [connections, setConnections] = useState([]);
  const [draggingCompId, setDraggingCompId] = useState(null);
  const [dragOffset, setDragOffset] = useState({ x: 0, y: 0 });
  const [linking, setLinking] = useState(null); // { fromId, type: 'input'|'output', index }
  const [hoveredPort, setHoveredPort] = useState(null);
  const [viewOffset, setViewOffset] = useState({ x: 0, y: 0 });
  const [isPanning, setIsPanning] = useState(false);
  
  const containerRef = useRef(null);

  // --- Logic Computation ---
  const [signalState, setSignalState] = useState({});

  const computeLogic = useCallback(() => {
    const states = {};
    
    // Initialize inputs
    components.forEach(c => {
      if (c.type === 'INPUT') {
        states[c.id] = c.value || false;
      }
    });

    let changed = true;
    let iterations = 0;
    const MAX_ITER = 50; // Prevent infinite loops in cyclic circuits

    while (changed && iterations < MAX_ITER) {
      changed = false;
      iterations++;

      components.forEach(comp => {
        if (comp.type === 'INPUT') return;

        const incomingSignals = [];
        for (let i = 0; i < GATE_TYPES[comp.type].inputs; i++) {
          const conn = connections.find(conn => conn.toId === comp.id && conn.toIndex === i);
          incomingSignals.push(conn ? states[conn.fromId] || false : false);
        }

        let result = false;
        switch (comp.type) {
          case 'AND': result = incomingSignals[0] && incomingSignals[1]; break;
          case 'OR':  result = incomingSignals[0] || incomingSignals[1]; break;
          case 'NOT': result = !incomingSignals[0]; break;
          case 'NAND': result = !(incomingSignals[0] && incomingSignals[1]); break;
          case 'NOR':  result = !(incomingSignals[0] || incomingSignals[1]); break;
          case 'XOR':  result = incomingSignals[0] !== incomingSignals[1]; break;
          case 'OUTPUT': result = incomingSignals[0]; break;
          default: result = false;
        }

        if (states[comp.id] !== result) {
          states[comp.id] = result;
          changed = true;
        }
      });
    }
    setSignalState(states);
  }, [components, connections]);

  useEffect(() => {
    computeLogic();
  }, [components, connections, computeLogic]);

  // --- Handlers ---
  const addComponent = (type) => {
    const newComp = {
      id: Math.random().toString(36).substr(2, 9),
      type,
      x: 100 - viewOffset.x,
      y: 100 - viewOffset.y,
      value: false, // For INPUT type
    };
    setComponents([...components, newComp]);
  };

  const deleteComponent = (id) => {
    setComponents(components.filter(c => c.id !== id));
    setConnections(connections.filter(conn => conn.fromId !== id && conn.toId !== id));
  };

  const toggleInput = (id) => {
    setComponents(components.map(c => 
      c.id === id ? { ...c, value: !c.value } : c
    ));
  };

  const onMouseDown = (e, comp) => {
    if (e.button !== 0) return;
    e.stopPropagation();
    setDraggingCompId(comp.id);
    const rect = e.currentTarget.getBoundingClientRect();
    setDragOffset({
      x: e.clientX - rect.left,
      y: e.clientY - rect.top
    });
  };

  const onPortMouseDown = (e, compId, type, index) => {
    e.stopPropagation();
    if (type === 'input') {
      // Remove existing connection to this input if exists
      setConnections(connections.filter(c => !(c.toId === compId && c.toIndex === index)));
    }
    setLinking({ fromId: compId, type, index });
  };

  const onMouseMove = (e) => {
    const rect = containerRef.current.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    if (draggingCompId) {
      setComponents(components.map(c => 
        c.id === draggingCompId 
          ? { ...c, x: Math.round((x - dragOffset.x) / 10) * 10, y: Math.round((y - dragOffset.y) / 10) * 10 } 
          : c
      ));
    }

    if (isPanning) {
      setViewOffset(prev => ({
        x: prev.x + e.movementX,
        y: prev.y + e.movementY
      }));
    }
  };

  const onMouseUp = (e) => {
    if (linking) {
      // Logic for completing a link is handled via onPortMouseUp on individual ports
      setLinking(null);
    }
    setDraggingCompId(null);
    setIsPanning(false);
  };

  const onPortMouseUp = (compId, type, index) => {
    if (linking && linking.fromId !== compId && linking.type !== type) {
      const from = linking.type === 'output' ? linking : { fromId: compId, type, index };
      const to = linking.type === 'input' ? linking : { fromId: compId, type, index };

      // Ensure we don't duplicate or overwrite wrongly
      setConnections(prev => [
        ...prev.filter(c => !(c.toId === to.fromId && c.toIndex === to.index)),
        { 
          fromId: from.fromId, 
          toId: to.fromId, 
          toIndex: to.index,
          id: Math.random().toString(36).substr(2, 5)
        }
      ]);
    }
  };

  // --- Helpers ---
  const getPortPos = (comp, type, index) => {
    const width = 80;
    const height = 50;
    if (type === 'input') {
      const inputs = GATE_TYPES[comp.type].inputs;
      const step = height / (inputs + 1);
      return { x: comp.x, y: comp.y + step * (index + 1) };
    } else {
      return { x: comp.x + width, y: comp.y + height / 2 };
    }
  };

  return (
    <div className="flex flex-col h-screen w-full bg-slate-900 text-slate-100 font-sans select-none overflow-hidden">
      {/* Header */}
      <header className="flex items-center justify-between px-6 py-4 bg-slate-800 border-b border-slate-700 shadow-lg">
        <div className="flex items-center gap-3">
          <div className="p-2 bg-indigo-600 rounded-lg">
            <Zap size={20} className="text-white" />
          </div>
          <h1 className="text-xl font-bold tracking-tight">LogicFlow <span className="text-xs font-normal text-slate-400">v1.0</span></h1>
        </div>
        <div className="flex items-center gap-4 text-sm text-slate-400">
          <div className="flex items-center gap-1"><Info size={14}/> 端子をドラッグして接続</div>
          <div className="flex items-center gap-1"><MousePointer2 size={14}/> スイッチをクリックでON/OFF</div>
        </div>
      </header>

      <div className="flex flex-1 overflow-hidden">
        {/* Sidebar */}
        <aside className="w-64 bg-slate-800 border-r border-slate-700 p-4 flex flex-col gap-6">
          <div>
            <h2 className="text-xs font-semibold text-slate-500 uppercase tracking-wider mb-3">Gates</h2>
            <div className="grid grid-cols-2 gap-2">
              {['AND', 'OR', 'NOT', 'NAND', 'NOR', 'XOR'].map(type => (
                <button 
                  key={type}
                  onClick={() => addComponent(type)}
                  className="flex items-center justify-center p-3 bg-slate-700 hover:bg-indigo-600 rounded-md transition-colors border border-slate-600 font-bold text-xs"
                >
                  {type}
                </button>
              ))}
            </div>
          </div>

          <div>
            <h2 className="text-xs font-semibold text-slate-500 uppercase tracking-wider mb-3">IO Components</h2>
            <div className="flex flex-col gap-2">
              <button 
                onClick={() => addComponent('INPUT')}
                className="flex items-center gap-3 px-4 py-2 bg-emerald-700 hover:bg-emerald-600 rounded-md transition-colors border border-emerald-600 text-sm font-medium"
              >
                <Plus size={16} /> Input Switch
              </button>
              <button 
                onClick={() => addComponent('OUTPUT')}
                className="flex items-center gap-3 px-4 py-2 bg-amber-700 hover:bg-amber-600 rounded-md transition-colors border border-amber-600 text-sm font-medium"
              >
                <Plus size={16} /> Output LED
              </button>
            </div>
          </div>

          <div className="mt-auto">
            <button 
              onClick={() => { setComponents([]); setConnections([]); }}
              className="w-full flex items-center justify-center gap-2 p-2 text-slate-400 hover:text-red-400 hover:bg-red-900/20 rounded-md transition-all text-xs border border-transparent hover:border-red-900/50"
            >
              <Trash2 size={14} /> Clear All
            </button>
          </div>
        </aside>

        {/* Canvas Area */}
        <main 
          ref={containerRef}
          className="flex-1 relative bg-slate-900 overflow-hidden cursor-crosshair"
          onMouseMove={onMouseMove}
          onMouseUp={onMouseUp}
          onMouseDown={(e) => { if (e.target === containerRef.current) setIsPanning(true); }}
        >
          {/* Grid Background */}
          <div 
            className="absolute inset-0 pointer-events-none opacity-20"
            style={{ 
              backgroundImage: `radial-gradient(circle, #4f46e5 1px, transparent 1px)`,
              backgroundSize: '20px 20px',
              transform: `translate(${viewOffset.x % 20}px, ${viewOffset.y % 20}px)`
            }}
          />

          <div style={{ transform: `translate(${viewOffset.x}px, ${viewOffset.y}px)` }}>
            {/* Connections */}
            <svg className="absolute inset-0 pointer-events-none w-[5000px] h-[5000px]">
              {connections.map(conn => {
                const fromComp = components.find(c => c.id === conn.fromId);
                const toComp = components.find(c => c.id === conn.toId);
                if (!fromComp || !toComp) return null;

                const start = getPortPos(fromComp, 'output', 0);
                const end = getPortPos(toComp, 'input', conn.toIndex);
                const isActive = signalState[fromComp.id];

                // Cubic Bezier curve for wires
                const dx = Math.abs(end.x - start.x) * 0.5;
                const path = `M ${start.x} ${start.y} C ${start.x + dx} ${start.y}, ${end.x - dx} ${end.y}, ${end.x} ${end.y}`;

                return (
                  <path 
                    key={conn.id}
                    d={path}
                    fill="none"
                    stroke={isActive ? '#10b981' : '#475569'}
                    strokeWidth={3}
                    className="transition-all duration-200"
                  />
                );
              })}
              
              {/* Active linking line */}
              {linking && (
                <line 
                  x1={getPortPos(components.find(c => c.id === linking.fromId), linking.type, linking.index).x}
                  y1={getPortPos(components.find(c => c.id === linking.fromId), linking.type, linking.index).y}
                  x2={linking.currentX || 0} // Would need mouse tracking relative to offset
                  y2={linking.currentY || 0}
                  stroke="#6366f1"
                  strokeWidth={2}
                  strokeDasharray="4"
                />
              )}
            </svg>

            {/* Components */}
            {components.map(comp => (
              <div
                key={comp.id}
                onMouseDown={(e) => onMouseDown(e, comp)}
                className={`absolute w-20 h-[50px] bg-slate-800 rounded-lg border-2 flex flex-col items-center justify-center transition-shadow cursor-grab active:cursor-grabbing ${
                  draggingCompId === comp.id ? 'border-indigo-500 shadow-xl scale-105 z-50' : 'border-slate-700 shadow-md'
                }`}
                style={{ left: comp.x, top: comp.y }}
              >
                {/* Delete Button (Small) */}
                <button 
                  onMouseDown={e => e.stopPropagation()}
                  onClick={() => deleteComponent(comp.id)}
                  className="absolute -top-2 -right-2 bg-red-500 rounded-full p-1 opacity-0 group-hover:opacity-100 hover:bg-red-400 transition-opacity"
                  style={{ opacity: draggingCompId === comp.id ? 0 : undefined }}
                >
                  <Trash2 size={10} color="white" />
                </button>

                {/* Ports: Inputs */}
                {Array.from({ length: GATE_TYPES[comp.type].inputs }).map((_, i) => (
                  <div
                    key={`in-${i}`}
                    onMouseDown={(e) => onPortMouseDown(e, comp.id, 'input', i)}
                    onMouseUp={() => onPortMouseUp(comp.id, 'input', i)}
                    className="absolute w-4 h-4 bg-slate-600 rounded-full border-2 border-slate-900 hover:bg-indigo-400 transition-colors cursor-crosshair -left-2 flex items-center justify-center"
                    style={{ top: `${(100 / (GATE_TYPES[comp.type].inputs + 1)) * (i + 1)}%`, transform: 'translateY(-50%)' }}
                  >
                    <div className="w-1 h-1 bg-white rounded-full opacity-50" />
                  </div>
                ))}

                {/* Ports: Output */}
                {comp.type !== 'OUTPUT' && (
                  <div
                    onMouseDown={(e) => onPortMouseDown(e, comp.id, 'output', 0)}
                    onMouseUp={() => onPortMouseUp(comp.id, 'output', 0)}
                    className="absolute w-4 h-4 bg-slate-600 rounded-full border-2 border-slate-900 hover:bg-indigo-400 transition-colors cursor-crosshair -right-2 top-1/2 -translate-y-1/2 flex items-center justify-center"
                  >
                    <div className="w-1 h-1 bg-white rounded-full opacity-50" />
                  </div>
                )}

                {/* Label & Icon */}
                <div className="flex flex-col items-center gap-0.5 pointer-events-none">
                  <span className="text-[10px] font-bold text-slate-400 uppercase tracking-tighter">
                    {GATE_TYPES[comp.type].label}
                  </span>
                  
                  {comp.type === 'INPUT' ? (
                    <button 
                      onMouseDown={e => e.stopPropagation()}
                      onClick={() => toggleInput(comp.id)}
                      className={`mt-1 pointer-events-auto w-10 h-5 rounded-full relative transition-colors ${comp.value ? 'bg-emerald-500' : 'bg-slate-600'}`}
                    >
                      <div className={`absolute top-1 w-3 h-3 bg-white rounded-full transition-all ${comp.value ? 'left-6' : 'left-1'}`} />
                    </button>
                  ) : comp.type === 'OUTPUT' ? (
                    <div className={`w-6 h-6 rounded-full border-2 transition-all duration-300 ${signalState[comp.id] ? 'bg-amber-400 border-amber-200 shadow-[0_0_15px_rgba(251,191,36,0.6)]' : 'bg-slate-700 border-slate-600'}`} />
                  ) : (
                    <Zap size={14} className={signalState[comp.id] ? 'text-emerald-400' : 'text-slate-600'} />
                  )}
                </div>
              </div>
            ))}
          </div>
        </main>
      </div>
      
      {/* Footer Info */}
      <footer className="bg-slate-800 border-t border-slate-700 px-4 py-2 flex justify-between items-center text-[10px] text-slate-500 uppercase tracking-widest">
        <div>Components: {components.length}</div>
        <div>Active Signals: {Object.values(signalState).filter(Boolean).length}</div>
        <div>Canvas Mode: Simulation Running</div>
      </footer>
    </div>
  );
};

export default App;
