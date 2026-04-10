#include "VisServer.hpp"

#include "buffer/ArrayBuffer.hpp"
#include "buffer/BufferVisitor.hpp"
#include "buffer/PieceTable.hpp"
#include "buffer/RopeBuffer.hpp"

#include <httplib.h>
#include <sstream>
#include <string>
#include <vector>

// ---- JSON helpers -------------------------------------------------------

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

// Display label for a rope node's character
static std::string charLabel(char c) {
    if (c == '\0')
        return "EOF";
    if (c == '\n')
        return "NL";
    if (c == ' ')
        return "\xc2\xb7"; // UTF-8 middle dot ·
    return std::string(1, c);
}

// ---- Concrete visitor ---------------------------------------------------

class JsonVizVisitor : public BufferVisitor {
public:
    std::string result;

    // ArrayBuffer: serialize as line array (heat-map view)
    void visit(const ArrayBuffer& buf, const EditorCtx& ctx) override {
        const auto& lines = linesOf(buf);

        std::ostringstream o;
        o << "{\n";
        o << "  \"bufferType\": \"" << jsonEscape(ctx.bufferType) << "\",\n";
        o << "  \"filePath\": \"" << jsonEscape(ctx.filePath) << "\",\n";
        o << "  \"isDirty\": " << (ctx.isDirty ? "true" : "false") << ",\n";
        o << "  \"cursor\": { \"row\": " << ctx.cursor.row << ", \"col\": " << ctx.cursor.col
          << " },\n";
        o << "  \"lineCount\": " << lines.size() << ",\n";
        o << "  \"lines\": [\n";
        for (std::size_t i = 0; i < lines.size(); ++i) {
            o << "    { \"index\": " << i << ", \"text\": \"" << jsonEscape(lines[i]) << "\""
              << ", \"length\": " << lines[i].size() << " }";
            if (i + 1 < lines.size())
                o << ",";
            o << "\n";
        }
        o << "  ]\n}\n";
        result = o.str();
    }

    // RopeBuffer: serialize as treap node array (tree view)
    // Layout: x = in-order index (= char position in text), y = depth.
    // This is the natural Cartesian-tree layout — BST shape is immediately visible.
    void visit(const RopeBuffer& buf, const EditorCtx& ctx) override {
        const RopeBufferNode* root = rootOf(buf);

        std::ostringstream o;
        o << "{\n";
        o << "  \"bufferType\": \"" << jsonEscape(ctx.bufferType) << "\",\n";
        o << "  \"filePath\": \"" << jsonEscape(ctx.filePath) << "\",\n";
        o << "  \"isDirty\": " << (ctx.isDirty ? "true" : "false") << ",\n";
        o << "  \"cursor\": { \"row\": " << ctx.cursor.row << ", \"col\": " << ctx.cursor.col
          << " },\n";
        o << "  \"rootId\": " << reinterpret_cast<std::size_t>(root) << ",\n";
        o << "  \"nodes\": [\n";

        // DFS — track in-order index (x) via a counter passed by ref
        std::size_t inorderIdx = 0;
        bool        first      = true;
        dfs(o, root, 0, inorderIdx, first);

        o << "\n  ]\n}\n";
        result = o.str();
    }

    // PieceTable: serialize backing string + pieces in document order.
    // Pieces behind cursor are colored blue in the frontend; after = amber.
    // The cursor sits at the boundary between the two sets.
    void visit(const PieceTable& buf, const EditorCtx& ctx) override {
        const std::string&     str       = strOf(buf);
        std::vector<PieceInfo> pieces    = piecesOf(buf);
        std::size_t            numBehind = numBehindOf(buf);
        std::size_t relOffset = ctx.cursorHandle ? buf.cursorRelOffset(*ctx.cursorHandle) : 0;

        std::ostringstream o;
        o << "{\n";
        o << "  \"bufferType\": \"" << jsonEscape(ctx.bufferType) << "\",\n";
        o << "  \"filePath\": \"" << jsonEscape(ctx.filePath) << "\",\n";
        o << "  \"isDirty\": " << (ctx.isDirty ? "true" : "false") << ",\n";
        o << "  \"cursor\": { \"row\": " << ctx.cursor.row << ", \"col\": " << ctx.cursor.col
          << " },\n";
        o << "  \"strLen\": " << str.size() << ",\n";
        o << "  \"numBehind\": " << numBehind << ",\n";
        o << "  \"cursorRelOffset\": " << relOffset << ",\n";
        o << "  \"str\": \"" << jsonEscape(str) << "\",\n";
        o << "  \"pieces\": [\n";

        for (std::size_t i = 0; i < pieces.size(); ++i) {
            const PieceInfo& p = pieces[i];
            std::string      content =
                (p.start <= str.size()) ? str.substr(p.start, p.end - p.start) : "";

            o << "    {"
              << " \"idx\": " << i << ", \"behind\": " << (p.behind ? "true" : "false")
              << ", \"start\": " << p.start << ", \"end\": " << p.end
              << ", \"len\": " << (p.end - p.start) << ", \"content\": \"" << jsonEscape(content)
              << "\""
              << ", \"newlines\": [";
            for (std::size_t j = 0; j < p.newlines.size(); ++j) {
                if (j > 0)
                    o << ", ";
                o << p.newlines[j];
            }
            o << "] }";
            if (i + 1 < pieces.size())
                o << ",";
            o << "\n";
        }

        o << "  ]\n}\n";
        result = o.str();
    }

private:
    void dfs(std::ostringstream& o, const RopeBufferNode* node, std::size_t depth,
             std::size_t& inorderIdx, bool& first) const {
        if (!node)
            return;
        dfs(o, node->left, depth + 1, inorderIdx, first);

        if (!first)
            o << ",\n";
        first = false;

        std::size_t myIdx = inorderIdx++;
        o << "    {"
          << " \"id\": " << reinterpret_cast<std::size_t>(node) << ", \"label\": \""
          << jsonEscape(charLabel(node->c)) << "\""
          << ", \"priority\": " << node->priority << ", \"numChars\": " << node->num_chars
          << ", \"numNewlines\": " << node->num_newlines
          << ", \"leftId\": " << reinterpret_cast<std::size_t>(node->left)
          << ", \"rightId\": " << reinterpret_cast<std::size_t>(node->right)
          << ", \"depth\": " << depth << ", \"x\": " << myIdx << " }";

        dfs(o, node->right, depth + 1, inorderIdx, first);
    }
};

// ---- VisServer ---------------------------------------------------------

VisServer::VisServer(Editor& editor, int port) : editor_(editor), port_(port) {}

VisServer::~VisServer() {
    stop();
}

void VisServer::start() {
    running_ = true;
    thread_  = std::thread(&VisServer::serveLoop, this);
}

void VisServer::stop() {
    if (running_.exchange(false) && svr_) {
        svr_->stop();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void VisServer::serveLoop() {
    httplib::Server svr;
    svr_ = &svr;

    svr.Get("/", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(buildHtml(), "text/html");
    });

    svr.Get("/api/state", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(buildJson(), "application/json");
    });

    svr.listen("localhost", port_);
    svr_ = nullptr;
}

std::string VisServer::buildJson() const {
    JsonVizVisitor visitor;
    editor_.acceptVisitor(visitor);
    return visitor.result;
}

std::string VisServer::buildHtml() const {
    return R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Buffer Visualizer</title>
<style>
  * { box-sizing: border-box; }
  body { font-family: monospace; background: #1e1e1e; color: #d4d4d4;
         margin: 0; padding: 12px; height: 100vh; display: flex;
         flex-direction: column; }
  #info { background: #2d2d2d; padding: 8px 12px; border-radius: 4px;
          margin-bottom: 10px; font-size: 13px; color: #9cdcfe; flex-shrink: 0; }

  /* ---- Array panel ---- */
  #array-panel { flex: 1; overflow-y: auto; }
  #buffer { white-space: pre; }
  .line { padding: 1px 8px; border-radius: 3px; margin: 1px 0;
          border-left: 3px solid transparent; }
  .cursor-line { border-left: 3px solid #fff; }
  #legend { margin-top: 12px; font-size: 11px; color: #888; }
  .swatch { display: inline-block; width: 14px; height: 14px;
            border-radius: 2px; vertical-align: middle; margin: 0 4px; }

  /* ---- Rope panel ---- */
  #rope-panel { flex: 1; overflow: hidden; position: relative; }
  #rope-svg { width: 100%; height: 100%; cursor: grab; }
  #rope-svg:active { cursor: grabbing; }
  #tooltip { position: absolute; background: #2d2d2d; border: 1px solid #555;
             padding: 6px 10px; border-radius: 4px; font-size: 11px;
             pointer-events: none; display: none; white-space: pre; }
  #rope-legend { position: absolute; bottom: 8px; left: 12px; font-size: 11px;
                 color: #888; }

  /* ---- Piece table panel ---- */
  #piece-panel { flex: 1; overflow-y: auto; padding: 4px 0; }
  .pt-section-label { color: #888; font-size: 11px; margin: 10px 0 4px; text-transform: uppercase; letter-spacing: 1px; }
  .pt-str { display: flex; flex-wrap: wrap; align-items: center; gap: 2px; margin-bottom: 4px; }
  .pt-char {
    display: inline-flex; align-items: center; justify-content: center;
    width: 22px; height: 28px; font-size: 14px; border-radius: 2px;
    border-left: 2px solid transparent; cursor: default; user-select: none;
  }
  .pt-char.behind  { background: #1a2e40; color: #9cdcfe; }
  .pt-char.after   { background: #2d1f00; color: #ffd080; }
  .pt-char.piece-start { border-left-color: #555; }
  .pt-char.cursor-piece {
    background: #2a5a8a; color: #ffffff;
    box-shadow: inset 0 2px 0 #3ddc84, inset 0 -2px 0 #3ddc84;
  }
  .pt-tape-caret {
    width: 3px; height: 30px; background: #3ddc84; border-radius: 1px;
    align-self: center; box-shadow: 0 0 4px #3ddc84;
  }
  .pt-pieces { display: flex; flex-direction: column; gap: 3px; }
  .pt-piece {
    display: flex; align-items: center; gap: 10px; padding: 6px 10px;
    border-radius: 4px; border-left: 4px solid transparent; font-size: 13px;
  }
  .pt-piece.behind { background: #1a2e40; border-left-color: #4e9af1; }
  .pt-piece.after  { background: #2d1f00; border-left-color: #f0a500; }
  .pt-piece.is-cursor { border-left-color: #3ddc84; background: #1f3a58; }
  .pt-piece-idx    { color: #666; min-width: 24px; }
  .pt-piece-side   { min-width: 52px; font-weight: bold; }
  .pt-piece.behind .pt-piece-side { color: #4e9af1; }
  .pt-piece.after  .pt-piece-side { color: #f0a500; }
  .pt-piece-range  { color: #666; min-width: 90px; font-size: 11px; }
  .pt-piece-content {
    flex: 1; color: #d4d4d4; overflow: hidden; text-overflow: ellipsis;
    white-space: nowrap; font-size: 16px; letter-spacing: 0.5px;
  }
  .pt-piece-caret {
    color: #3ddc84; font-weight: bold;
    text-shadow: 0 0 6px #3ddc84;
    display: inline-block; margin: 0 -1px;
  }
  .pt-cursor-badge { color: #3ddc84; font-size: 11px; white-space: nowrap; }
  .pt-cursor-divider {
    text-align: center; color: #3ddc84; font-size: 11px;
    border: 1px dashed #3ddc84; border-radius: 3px; padding: 2px 0; margin: 2px 0;
  }
  #piece-legend { margin-top: 12px; font-size: 11px; color: #888; }
</style>
</head>
<body>
<div id="info">Loading...</div>

<!-- Array view -->
<div id="array-panel">
  <div id="buffer"></div>
  <div id="legend">
    Line colour: length heat-map &nbsp;
    <span class="swatch" style="background:hsl(120,55%,30%)"></span>short
    <span class="swatch" style="background:hsl(60,55%,30%)"></span>medium
    <span class="swatch" style="background:hsl(0,55%,30%)"></span>long
  </div>
</div>

<!-- Rope tree view -->
<div id="rope-panel" style="display:none">
  <svg id="rope-svg" xmlns="http://www.w3.org/2000/svg">
    <g id="rope-g"></g>
  </svg>
  <div id="tooltip"></div>
  <div id="rope-legend">
    <span style="color:#4e9af1">&#9679;</span> char &nbsp;
    <span style="color:#f0a500">&#9679;</span> newline (NL) &nbsp;
    <span style="color:#888">&#9679;</span> EOF &nbsp;
    <span style="color:#3ddc84">&#9679;</span> cursor &nbsp;
    x = text position &nbsp; y = depth
  </div>
</div>

<!-- Piece table view -->
<div id="piece-panel" style="display:none">
  <div class="pt-section-label">Backing string</div>
  <div id="pt-str" class="pt-str"></div>
  <div class="pt-section-label">Pieces (document order)</div>
  <div id="pt-pieces" class="pt-pieces"></div>
  <div id="piece-legend">
    <span style="color:#4e9af1">&#9679;</span> behind cursor &nbsp;
    <span style="color:#f0a500">&#9679;</span> after cursor &nbsp;
    <span style="color:#3ddc84">&#9679;</span> cursor piece + caret &nbsp;
    | = piece boundary
  </div>
</div>

<script>
// ---- pan/zoom state ----
let vpX = 40, vpY = 40, vpScale = 1;
let dragging = false, dragStartX, dragStartY, dragVpX, dragVpY;

const svg    = document.getElementById('rope-svg');
const g      = document.getElementById('rope-g');
const tip    = document.getElementById('tooltip');

svg.addEventListener('mousedown', e => {
  dragging = true; dragStartX = e.clientX; dragStartY = e.clientY;
  dragVpX = vpX; dragVpY = vpY;
});
window.addEventListener('mouseup',   () => { dragging = false; });
window.addEventListener('mousemove', e => {
  if (!dragging) return;
  vpX = dragVpX + (e.clientX - dragStartX);
  vpY = dragVpY + (e.clientY - dragStartY);
  applyTransform();
});
svg.addEventListener('wheel', e => {
  e.preventDefault();
  const factor = e.deltaY < 0 ? 1.1 : 0.9;
  const rect = svg.getBoundingClientRect();
  const mx = e.clientX - rect.left, my = e.clientY - rect.top;
  vpX = mx - (mx - vpX) * factor;
  vpY = my - (my - vpY) * factor;
  vpScale *= factor;
  applyTransform();
}, { passive: false });

function applyTransform() {
  g.setAttribute('transform', `translate(${vpX},${vpY}) scale(${vpScale})`);
}

// ---- layout constants ----
const NR = 14;   // node radius
const XS = 34;   // x spacing (in-order index step)
const YS = 60;   // y spacing (depth step)

function renderRope(s) {
  const map = {};
  s.nodes.forEach(n => { map[n.id] = n; });

  // Build SVG string — edges first, then nodes
  let edges = '', nodes = '';

  s.nodes.forEach(n => {
    const nx = n.x * XS, ny = n.depth * YS;

    // edges to children
    ['leftId','rightId'].forEach(k => {
      if (n[k]) {
        const ch = map[n[k]];
        if (ch) {
          const cx = ch.x * XS, cy = ch.depth * YS;
          edges += `<line x1="${nx}" y1="${ny}" x2="${cx}" y2="${cy}"
                         stroke="#555" stroke-width="1.5"/>`;
        }
      }
    });

    // node circle
    const isCursor = (n.id === s.cursorId);
    let fill = '#4e9af1';
    if (n.label === 'EOF') fill = '#888';
    else if (n.label === 'NL') fill = '#f0a500';
    const stroke = isCursor ? '#3ddc84' : '#1e1e1e';
    const sw     = isCursor ? 3 : 1;
    const r      = isCursor ? NR + 2 : NR;

    // sanitise label for SVG text (& → entity)
    const lbl = n.label.replace(/&/g,'&amp;').replace(/</g,'&lt;');

    nodes += `<g class="rope-node" data-id="${n.id}"
                transform="translate(${nx},${ny})">
      <circle r="${r}" fill="${fill}" stroke="${stroke}" stroke-width="${sw}"/>
      <text text-anchor="middle" dominant-baseline="central"
            font-size="11" fill="#fff">${lbl}</text>
    </g>`;
  });

  g.innerHTML = edges + nodes;

  // Tooltip on hover
  g.querySelectorAll('.rope-node').forEach(el => {
    const n = map[el.dataset.id];
    el.addEventListener('mouseenter', ev => {
      tip.style.display = 'block';
      tip.textContent =
        `char:        ${n.label}\n` +
        `priority:    ${n.priority}\n` +
        `num_chars:   ${n.numChars}\n` +
        `num_newlines:${n.numNewlines}\n` +
        `depth:       ${n.depth}\n` +
        `x (pos):     ${n.x}`;
    });
    el.addEventListener('mousemove', ev => {
      const r = document.getElementById('rope-panel').getBoundingClientRect();
      tip.style.left = (ev.clientX - r.left + 12) + 'px';
      tip.style.top  = (ev.clientY - r.top  + 12) + 'px';
    });
    el.addEventListener('mouseleave', () => { tip.style.display = 'none'; });
  });
}

function renderPiece(s) {
  const escapeHtml = t => t.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');

  // ---- identify cursor piece (= last behind piece) ----
  // Cursor sits at rel_offset within this piece.
  const cursorPieceIdx = s.numBehind > 0 ? s.numBehind - 1 : -1;
  const cursorPiece    = cursorPieceIdx >= 0 ? s.pieces[cursorPieceIdx] : null;
  const relOffset      = s.cursorRelOffset || 0;
  // Absolute position in the backing string where the cursor points.
  // In state B (rel_offset == piece.length), this equals cursorPiece.end.
  const cursorAbs      = cursorPiece ? (cursorPiece.start + relOffset) : -1;

  // ---- backing string character tape ----
  const strEl = document.getElementById('pt-str');
  strEl.innerHTML = '';

  // Build a map: abs_position -> {behind, pieceStart, isCursorPiece}
  const posInfo = new Array(s.str.length).fill(null).map(() => ({
    behind: false, pieceStart: false, isCursorPiece: false
  }));
  s.pieces.forEach((p, pi) => {
    for (let i = p.start; i < p.end; i++) {
      posInfo[i].behind = p.behind;
      if (i === p.start) posInfo[i].pieceStart = true;
      if (pi === cursorPieceIdx) posInfo[i].isCursorPiece = true;
    }
  });

  const appendCaret = () => {
    const caret = document.createElement('span');
    caret.className = 'pt-tape-caret';
    caret.title = 'cursor (rel_offset=' + relOffset + ')';
    strEl.appendChild(caret);
  };

  for (let i = 0; i < s.str.length; i++) {
    // Insert tape cursor marker before this char if cursor is at position i.
    if (i === cursorAbs) appendCaret();

    const c = s.str[i];
    const info = posInfo[i];
    const span = document.createElement('span');
    let lbl = c === '\n' ? '\\n' : (c === ' ' ? '\u00b7' : c);
    span.textContent = lbl;
    let cls = 'pt-char ' + (info.behind ? 'behind' : 'after');
    if (info.pieceStart)   cls += ' piece-start';
    if (info.isCursorPiece) cls += ' cursor-piece';
    span.className = cls;
    span.title = 'pos ' + i + (c === '\n' ? ' (newline)' : '');
    strEl.appendChild(span);
  }
  // Trailing caret if cursor is past the last char of str (EOF / end-of-cursor-piece at end).
  if (cursorAbs === s.str.length) appendCaret();

  // ---- piece list ----
  const piecesEl = document.getElementById('pt-pieces');
  piecesEl.innerHTML = '';

  let cursorInserted = false;
  s.pieces.forEach((p, pi) => {
    // Insert cursor divider between last behind piece and first after piece
    if (!cursorInserted && !p.behind) {
      const div = document.createElement('div');
      div.className = 'pt-cursor-divider';
      div.textContent = '\u25bc cursor \u25bc';
      piecesEl.appendChild(div);
      cursorInserted = true;
    }

    const row = document.createElement('div');
    const isCursorPiece = (pi === cursorPieceIdx);
    row.className = 'pt-piece ' + (p.behind ? 'behind' : 'after') +
                    (isCursorPiece ? ' is-cursor' : '');

    // content display: replace \n with visible marker
    const contentDisplay = p.content.replace(/\n/g, '\u21b5');

    // For the cursor piece, splice a caret into the content at rel_offset
    // (each byte of p.content maps 1:1 to a visible char, incl. \n → ↵).
    let contentHtml;
    if (isCursorPiece) {
      const clamped = Math.min(relOffset, contentDisplay.length);
      const before = contentDisplay.substring(0, clamped);
      const after  = contentDisplay.substring(clamped);
      contentHtml = escapeHtml(before) +
                    '<span class="pt-piece-caret">\u2502</span>' +
                    escapeHtml(after);
    } else {
      contentHtml = escapeHtml(contentDisplay);
    }

    const cursorInfo = isCursorPiece
      ? ' <span class="pt-cursor-badge">rel_offset=' + relOffset + '</span>'
      : '';

    row.innerHTML =
      '<span class="pt-piece-idx">#' + pi + '</span>' +
      '<span class="pt-piece-side">' + (p.behind ? 'behind' : 'after') + '</span>' +
      '<span class="pt-piece-range">[' + p.start + ', ' + p.end + ')</span>' +
      '<span class="pt-piece-content">' + contentHtml + '</span>' +
      '<span class="pt-piece-idx"> len=' + p.len + ' nl=' + p.newlines.length + '</span>' +
      cursorInfo;

    piecesEl.appendChild(row);
  });

  // If all pieces are behind (no after pieces), cursor divider goes at end
  if (!cursorInserted) {
    const div = document.createElement('div');
    div.className = 'pt-cursor-divider';
    div.textContent = '\u25bc cursor \u25bc';
    piecesEl.appendChild(div);
  }
}

function renderArray(s) {
  const fp = s.filePath || '[No Name]';
  const container = document.getElementById('buffer');
  container.innerHTML = '';
  s.lines.forEach((line, i) => {
    const div = document.createElement('div');
    const hue = Math.max(0, 120 - line.length * 2);
    const isCursor = (i === s.cursor.row);
    div.style.backgroundColor = 'hsl(' + hue + ',55%,' + (isCursor ? '25%' : '18%') + ')';
    div.className = 'line' + (isCursor ? ' cursor-line' : '');
    const idx = String(i).padStart(4, ' ');
    div.textContent = idx + ' \u2502 ' + (line.text || '');
    container.appendChild(div);
  });
}

async function refresh() {
  try {
    const resp = await fetch('/api/state');
    const s = await resp.json();

    const dirty = s.isDirty ? ' *' : '';
    const fp    = s.filePath || '[No Name]';
    const bt    = (s.bufferType || '').toLowerCase();
    const isRope  = bt.includes('rope');
    const isPiece = bt.includes('piece');

    let infoExtra;
    if (isRope)       infoExtra = '  Nodes: '  + (s.nodes  ? s.nodes.length  : 0);
    else if (isPiece) infoExtra = '  Pieces: ' + (s.pieces ? s.pieces.length : 0) +
                                  '  StrLen: ' + (s.strLen || 0);
    else              infoExtra = '  Lines: '  + (s.lineCount || (s.lines && s.lines.length) || 0);

    document.getElementById('info').textContent =
      fp + dirty + '  [' + s.bufferType + ']  ' +
      'Cursor ' + s.cursor.row + ':' + s.cursor.col + infoExtra;

    document.getElementById('array-panel').style.display = (!isRope && !isPiece) ? 'block' : 'none';
    document.getElementById('rope-panel').style.display  = isRope  ? 'block' : 'none';
    document.getElementById('piece-panel').style.display = isPiece ? 'block' : 'none';

    if (isRope)       renderRope(s);
    else if (isPiece) renderPiece(s);
    else              renderArray(s);
  } catch(e) {
    document.getElementById('info').textContent = 'Connection lost...';
  }
}

applyTransform();
setInterval(refresh, 50);
refresh();
</script>
</body>
</html>
)html";
}
