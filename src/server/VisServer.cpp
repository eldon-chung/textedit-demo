#include "VisServer.hpp"

#include "buffer/ArrayBuffer.hpp"
#include "buffer/BufferVisitor.hpp"

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
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// ---- Concrete visitor ---------------------------------------------------

// Visits the buffer directly and serialises its internals to JSON.
// Each buffer type gets its own visit() override — the viz adapts to the DS.
class JsonVizVisitor : public BufferVisitor {
public:
    std::string result;

    void visit(const ArrayBuffer& buf, const EditorCtx& ctx) override {
        const auto& lines = linesOf(buf);  // direct access via attorney accessor

        std::ostringstream o;
        o << "{\n";
        o << "  \"bufferType\": \"" << jsonEscape(ctx.bufferType) << "\",\n";
        o << "  \"filePath\": \""   << jsonEscape(ctx.filePath)   << "\",\n";
        o << "  \"isDirty\": "      << (ctx.isDirty ? "true" : "false") << ",\n";
        o << "  \"cursor\": { \"row\": " << ctx.cursor.row
                          << ", \"col\": " << ctx.cursor.col << " },\n";
        o << "  \"lineCount\": " << lines.size() << ",\n";
        o << "  \"lines\": [\n";
        for (std::size_t i = 0; i < lines.size(); ++i) {
            o << "    { \"index\": " << i
              << ", \"text\": \""   << jsonEscape(lines[i]) << "\""
              << ", \"length\": "   << lines[i].size() << " }";
            if (i + 1 < lines.size()) o << ",";
            o << "\n";
        }
        o << "  ]\n}\n";
        result = o.str();
    }
};

// ---- VisServer ---------------------------------------------------------

VisServer::VisServer(Editor& editor, int port)
    : editor_(editor), port_(port) {}

VisServer::~VisServer() {
    stop();
}

void VisServer::start() {
    running_ = true;
    thread_ = std::thread(&VisServer::serveLoop, this);
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
  body { font-family: monospace; background: #1e1e1e; color: #d4d4d4; margin: 0; padding: 12px; }
  #info { background: #2d2d2d; padding: 8px 12px; border-radius: 4px; margin-bottom: 10px;
          font-size: 13px; color: #9cdcfe; }
  #buffer { white-space: pre; }
  .line { padding: 1px 8px; border-radius: 3px; margin: 1px 0;
          border-left: 3px solid transparent; transition: background 0.15s; }
  .cursor-line { border-left: 3px solid #fff; }
  #legend { margin-top: 12px; font-size: 11px; color: #888; }
  .swatch { display: inline-block; width: 14px; height: 14px;
            border-radius: 2px; vertical-align: middle; margin: 0 4px; }
</style>
</head>
<body>
<div id="info">Loading...</div>
<div id="buffer"></div>
<div id="legend">
  Line colour: length heat-map &nbsp;
  <span class="swatch" style="background:hsl(120,55%,30%)"></span>short
  <span class="swatch" style="background:hsl(60,55%,30%)"></span>medium
  <span class="swatch" style="background:hsl(0,55%,30%)"></span>long
</div>
<script>
async function refresh() {
  try {
    const resp = await fetch('/api/state');
    const s = await resp.json();
    const dirty = s.isDirty ? ' *' : '';
    const fp = s.filePath || '[No Name]';
    document.getElementById('info').textContent =
      fp + dirty + '  [' + s.bufferType + ']  ' +
      'Cursor ' + s.cursor.row + ':' + s.cursor.col +
      '  Lines: ' + s.lineCount;

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
  } catch(e) {
    document.getElementById('info').textContent = 'Connection lost...';
  }
}
setInterval(refresh, 500);
refresh();
</script>
</body>
</html>
)html";
}
