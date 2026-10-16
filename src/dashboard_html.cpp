#define _CRT_SECURE_NO_WARNINGS
#include "taskmesh/server.hpp"

namespace taskmesh {

std::string TaskMeshServer::get_dashboard_html() {
    return R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>TaskMesh — Concurrent Priority Job Engine</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500;600&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-dark: #0a0d14;
      --bg-card: rgba(18, 24, 38, 0.75);
      --bg-card-hover: rgba(26, 34, 52, 0.85);
      --border-color: rgba(255, 255, 255, 0.08);
      --border-focus: #3b82f6;
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
      --primary: #3b82f6;
      --primary-glow: rgba(59, 130, 246, 0.35);
      --success: #10b981;
      --warning: #f59e0b;
      --danger: #ef4444;
      --purple: #8b5cf6;
      --critical: #ec4899;
      --font-sans: 'Inter', -apple-system, sans-serif;
      --font-mono: 'JetBrains Mono', monospace;
    }

    * { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      background-color: var(--bg-dark);
      background-image: 
        radial-gradient(circle at 15% 15%, rgba(59, 130, 246, 0.12) 0%, transparent 45%),
        radial-gradient(circle at 85% 85%, rgba(139, 92, 246, 0.10) 0%, transparent 45%);
      background-attachment: fixed;
      color: var(--text-main);
      font-family: var(--font-sans);
      min-height: 100vh;
      display: flex;
      flex-direction: column;
    }

    header {
      background: rgba(10, 13, 20, 0.85);
      backdrop-filter: blur(16px);
      border-bottom: 1px solid var(--border-color);
      position: sticky;
      top: 0;
      z-index: 50;
      padding: 0.85rem 2rem;
      display: flex;
      align-items: center;
      justify-content: space-between;
      flex-wrap: wrap;
      gap: 1rem;
    }

    .brand {
      display: flex;
      align-items: center;
      gap: 0.75rem;
      text-decoration: none;
      color: inherit;
    }

    .brand-icon {
      width: 36px;
      height: 36px;
      background: linear-gradient(135deg, #3b82f6, #8b5cf6);
      border-radius: 8px;
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 0 20px var(--primary-glow);
    }

    .brand-icon svg { width: 20px; height: 20px; fill: white; }
    .brand-title { font-size: 1.25rem; font-weight: 700; letter-spacing: -0.02em; }
    .brand-badge {
      font-size: 0.7rem;
      font-weight: 600;
      padding: 0.2rem 0.5rem;
      border-radius: 9999px;
      background: rgba(59, 130, 246, 0.15);
      border: 1px solid rgba(59, 130, 246, 0.3);
      color: #60a5fa;
      letter-spacing: 0.05em;
      text-transform: uppercase;
    }

    .header-actions {
      display: flex;
      align-items: center;
      gap: 0.75rem;
      flex-wrap: wrap;
    }

    .status-indicator {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      font-size: 0.85rem;
      color: var(--text-muted);
      background: rgba(255, 255, 255, 0.03);
      padding: 0.4rem 0.8rem;
      border-radius: 6px;
      border: 1px solid var(--border-color);
    }

    .pulse-dot {
      width: 8px;
      height: 8px;
      background: var(--success);
      border-radius: 50%;
      box-shadow: 0 0 8px var(--success);
      animation: pulse 2s infinite ease-in-out;
    }

    @keyframes pulse {
      0%, 100% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.4; transform: scale(0.8); }
    }

    .btn {
      display: inline-flex;
      align-items: center;
      gap: 0.5rem;
      padding: 0.45rem 0.85rem;
      border-radius: 6px;
      font-size: 0.85rem;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s;
      border: none;
      font-family: inherit;
    }

    .btn-primary {
      background: var(--primary);
      color: white;
      box-shadow: 0 0 15px var(--primary-glow);
    }
    .btn-primary:hover { background: #2563eb; transform: translateY(-1px); }

    .btn-secondary {
      background: rgba(255, 255, 255, 0.05);
      color: var(--text-main);
      border: 1px solid var(--border-color);
    }
    .btn-secondary:hover { background: rgba(255, 255, 255, 0.1); border-color: rgba(255, 255, 255, 0.2); }

    .btn-success {
      background: rgba(16, 185, 129, 0.15);
      color: #34d399;
      border: 1px solid rgba(16, 185, 129, 0.3);
    }
    .btn-success:hover { background: rgba(16, 185, 129, 0.25); }

    .btn-danger {
      background: rgba(239, 68, 68, 0.15);
      color: #f87171;
      border: 1px solid rgba(239, 68, 68, 0.3);
    }
    .btn-danger:hover { background: rgba(239, 68, 68, 0.25); }

    .btn-warning {
      background: rgba(245, 158, 11, 0.15);
      color: #fbbf24;
      border: 1px solid rgba(245, 158, 11, 0.3);
    }
    .btn-warning:hover { background: rgba(245, 158, 11, 0.25); }

    .btn-xs {
      padding: 0.25rem 0.55rem;
      font-size: 0.75rem;
      border-radius: 4px;
    }

    main {
      flex: 1;
      padding: 2rem;
      max-width: 1400px;
      margin: 0 auto;
      width: 100%;
    }

    /* Metrics Cards */
    .metrics-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 1.25rem;
      margin-bottom: 2rem;
    }

    .metric-card {
      background: var(--bg-card);
      backdrop-filter: blur(12px);
      border: 1px solid var(--border-color);
      border-radius: 12px;
      padding: 1.25rem;
      transition: all 0.2s;
    }

    .metric-card:hover {
      background: var(--bg-card-hover);
      transform: translateY(-2px);
    }

    .metric-label {
      font-size: 0.75rem;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.05em;
      margin-bottom: 0.5rem;
    }

    .metric-value {
      font-size: 1.75rem;
      font-weight: 700;
      font-family: var(--font-mono);
    }

    .card-submitted { border-top: 3px solid var(--primary); }
    .card-queued { border-top: 3px solid var(--warning); }
    .card-running { border-top: 3px solid var(--purple); }
    .card-completed { border-top: 3px solid var(--success); }
    .card-failed { border-top: 3px solid var(--danger); }
    .card-cancelled { border-top: 3px solid #94a3b8; }

    /* Toolbar & Search */
    .toolbar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 1.25rem;
      flex-wrap: wrap;
      gap: 1rem;
    }

    .search-box {
      position: relative;
      flex: 1;
      max-width: 380px;
      min-width: 240px;
    }

    .search-box input {
      width: 100%;
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--border-color);
      border-radius: 8px;
      padding: 0.55rem 0.85rem 0.55rem 2.2rem;
      color: var(--text-main);
      font-family: inherit;
      font-size: 0.85rem;
      transition: all 0.2s;
    }

    .search-box input:focus {
      outline: none;
      border-color: var(--primary);
      box-shadow: 0 0 10px var(--primary-glow);
    }

    .search-icon {
      position: absolute;
      left: 0.75rem;
      top: 50%;
      transform: translateY(-50%);
      color: var(--text-muted);
      pointer-events: none;
    }

    .filter-group {
      display: flex;
      align-items: center;
      gap: 0.4rem;
      flex-wrap: wrap;
    }

    .filter-pill {
      background: rgba(255, 255, 255, 0.03);
      border: 1px solid var(--border-color);
      color: var(--text-muted);
      padding: 0.35rem 0.75rem;
      border-radius: 9999px;
      font-size: 0.8rem;
      cursor: pointer;
      transition: all 0.2s;
    }

    .filter-pill:hover {
      background: rgba(255, 255, 255, 0.07);
      color: var(--text-main);
    }

    .filter-pill.active {
      background: var(--primary);
      border-color: var(--primary);
      color: white;
      font-weight: 600;
    }

    /* Table */
    .table-container {
      background: var(--bg-card);
      backdrop-filter: blur(12px);
      border: 1px solid var(--border-color);
      border-radius: 12px;
      overflow-x: auto;
    }

    table {
      width: 100%;
      border-collapse: collapse;
      text-align: left;
      font-size: 0.875rem;
    }

    th {
      background: rgba(255, 255, 255, 0.02);
      color: var(--text-muted);
      font-weight: 600;
      font-size: 0.75rem;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      padding: 0.85rem 1.25rem;
      border-bottom: 1px solid var(--border-color);
    }

    td {
      padding: 0.85rem 1.25rem;
      border-bottom: 1px solid rgba(255, 255, 255, 0.04);
      vertical-align: middle;
    }

    tr:hover td {
      background: rgba(255, 255, 255, 0.02);
    }

    .priority-badge {
      display: inline-block;
      font-size: 0.75rem;
      font-weight: 600;
      padding: 0.2rem 0.55rem;
      border-radius: 4px;
      font-family: var(--font-mono);
    }

    .priority-Low { background: rgba(148, 163, 184, 0.15); color: #94a3b8; }
    .priority-Normal { background: rgba(59, 130, 246, 0.15); color: #60a5fa; }
    .priority-High { background: rgba(245, 158, 11, 0.15); color: #fbbf24; }
    .priority-Critical { background: rgba(236, 72, 153, 0.15); color: #f472b6; }

    .status-badge {
      display: inline-flex;
      align-items: center;
      gap: 0.4rem;
      font-size: 0.75rem;
      font-weight: 600;
      padding: 0.2rem 0.6rem;
      border-radius: 9999px;
    }

    .status-Queued { background: rgba(245, 158, 11, 0.15); color: #fbbf24; }
    .status-Running { background: rgba(139, 92, 246, 0.15); color: #c084fc; }
    .status-Completed { background: rgba(16, 185, 129, 0.15); color: #34d399; }
    .status-Failed { background: rgba(239, 68, 68, 0.15); color: #f87171; }
    .status-Cancelled { background: rgba(148, 163, 184, 0.15); color: #94a3b8; }

    .code-text {
      font-family: var(--font-mono);
      font-size: 0.8rem;
      background: rgba(0, 0, 0, 0.25);
      padding: 0.2rem 0.4rem;
      border-radius: 4px;
      max-width: 220px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      display: inline-block;
      vertical-align: middle;
    }

    .error-snippet {
      display: block;
      color: #f87171;
      font-size: 0.75rem;
      margin-top: 0.25rem;
      max-width: 240px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    /* Modals */
    .modal-overlay {
      position: fixed;
      top: 0; left: 0; right: 0; bottom: 0;
      background: rgba(0, 0, 0, 0.8);
      backdrop-filter: blur(8px);
      display: none;
      align-items: center;
      justify-content: center;
      z-index: 100;
      padding: 1rem;
    }

    .modal-overlay.active { display: flex; }

    .modal-box {
      background: #111726;
      border: 1px solid var(--border-color);
      border-radius: 16px;
      max-width: 580px;
      width: 100%;
      padding: 1.75rem;
      box-shadow: 0 20px 50px rgba(0, 0, 0, 0.6);
      max-height: 90vh;
      overflow-y: auto;
    }

    .modal-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 1.25rem;
    }

    .modal-title { font-size: 1.2rem; font-weight: 700; }
    .modal-close {
      background: transparent;
      border: none;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 1.5rem;
      line-height: 1;
    }

    .form-group {
      margin-bottom: 1.1rem;
      display: flex;
      flex-direction: column;
      gap: 0.35rem;
    }

    label { font-size: 0.8rem; font-weight: 600; color: var(--text-muted); }

    input, select, textarea {
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--border-color);
      border-radius: 6px;
      padding: 0.55rem 0.75rem;
      color: var(--text-main);
      font-family: inherit;
      font-size: 0.875rem;
    }

    input:focus, select:focus, textarea:focus {
      outline: none;
      border-color: var(--primary);
      box-shadow: 0 0 10px var(--primary-glow);
    }

    textarea { resize: vertical; min-height: 80px; font-family: var(--font-mono); }

    .error-banner {
      background: rgba(239, 68, 68, 0.12);
      border: 1px solid rgba(239, 68, 68, 0.3);
      border-radius: 8px;
      padding: 0.75rem;
      margin-bottom: 1rem;
      color: #fca5a5;
      font-size: 0.85rem;
      line-height: 1.4;
      word-break: break-all;
    }

    .meta-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 0.75rem;
      margin-bottom: 1.25rem;
      background: rgba(255, 255, 255, 0.02);
      padding: 0.85rem;
      border-radius: 8px;
      border: 1px solid var(--border-color);
    }

    .meta-item { display: flex; flex-direction: column; gap: 0.2rem; }
    .meta-item span:first-child { font-size: 0.75rem; color: var(--text-muted); }
    .meta-item span:last-child { font-size: 0.875rem; font-weight: 600; }

    .payload-pre {
      background: #07090e;
      border: 1px solid var(--border-color);
      border-radius: 8px;
      padding: 0.85rem;
      font-family: var(--font-mono);
      font-size: 0.8rem;
      overflow-x: auto;
      max-height: 200px;
      color: #93c5fd;
      white-space: pre-wrap;
      word-break: break-all;
    }

    /* Toast Notifications */
    #toast-container {
      position: fixed;
      bottom: 1.5rem;
      right: 1.5rem;
      z-index: 200;
      display: flex;
      flex-direction: column;
      gap: 0.5rem;
    }

    .toast {
      background: #1e293b;
      border: 1px solid var(--border-color);
      color: var(--text-main);
      padding: 0.75rem 1.25rem;
      border-radius: 8px;
      box-shadow: 0 10px 25px rgba(0, 0, 0, 0.5);
      font-size: 0.85rem;
      display: flex;
      align-items: center;
      gap: 0.5rem;
      animation: slideIn 0.25s ease-out;
    }

    .toast-success { border-left: 4px solid var(--success); }
    .toast-error { border-left: 4px solid var(--danger); }
    .toast-info { border-left: 4px solid var(--primary); }

    @keyframes slideIn {
      from { transform: translateX(100%); opacity: 0; }
      to { transform: translateX(0); opacity: 1; }
    }
  </style>
</head>
<body>

  <header>
    <a href="/" class="brand">
      <div class="brand-icon">
        <svg viewBox="0 0 24 24"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg>
      </div>
      <div>
        <div class="brand-title">TaskMesh <span class="brand-badge">Engine v0.2.0</span></div>
      </div>
    </a>

    <div class="header-actions">
      <div class="status-indicator">
        <span class="pulse-dot"></span>
        <span id="engine-status">Engine Healthy</span>
      </div>
      <button class="btn btn-secondary" onclick="openApiKeyModal()">
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 2l-2 2m-2-2l2 2m7 0a9 9 0 1 1-18 0 9 9 0 0 1 18 0z"/><path d="M7 11l4.5 4.5"/></svg>
        <span id="api-key-status-text">API Key</span>
      </button>
      <button class="btn btn-secondary" onclick="fetchData()">
        <svg id="refresh-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67"/></svg>
        Refresh
      </button>
      <button class="btn btn-primary" onclick="openSubmitModal()">
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"></line><line x1="5" y1="12" x2="19" y2="12"></line></svg>
        Submit Job
      </button>
    </div>
  </header>

  <main>
    <!-- Metric Gauges -->
    <div class="metrics-grid">
      <div class="metric-card card-submitted">
        <div class="metric-label">Submitted</div>
        <div class="metric-value" id="val-submitted">0</div>
      </div>
      <div class="metric-card card-queued">
        <div class="metric-label">Queued</div>
        <div class="metric-value" id="val-queued">0</div>
      </div>
      <div class="metric-card card-running">
        <div class="metric-label">Running</div>
        <div class="metric-value" id="val-running">0</div>
      </div>
      <div class="metric-card card-completed">
        <div class="metric-label">Completed</div>
        <div class="metric-value" id="val-completed">0</div>
      </div>
      <div class="metric-card card-failed">
        <div class="metric-label">Failed</div>
        <div class="metric-value" id="val-failed">0</div>
      </div>
      <div class="metric-card card-cancelled">
        <div class="metric-label">Cancelled</div>
        <div class="metric-value" id="val-cancelled">0</div>
      </div>
    </div>

    <!-- Toolbar & Filters -->
    <div class="toolbar">
      <div class="search-box">
        <svg class="search-icon" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="8"></circle><line x1="21" y1="21" x2="16.65" y2="16.65"></line></svg>
        <input type="text" id="search-input" placeholder="Search by ID, type, or payload..." oninput="filterAndRender()">
      </div>

      <div class="filter-group">
        <button class="filter-pill active" onclick="setFilter(null, this)">All Jobs</button>
        <button class="filter-pill" onclick="setFilter('Queued', this)">Queued</button>
        <button class="filter-pill" onclick="setFilter('Running', this)">Running</button>
        <button class="filter-pill" onclick="setFilter('Completed', this)">Completed</button>
        <button class="filter-pill" onclick="setFilter('Failed', this)">Failed</button>
        <button class="filter-pill" onclick="setFilter('Cancelled', this)">Cancelled</button>
      </div>

      <div style="display:flex; align-items:center; gap:0.75rem; margin-left:auto;">
        <span style="font-size:0.8rem; color:var(--text-muted);">Auto-refresh:</span>
        <input type="checkbox" id="auto-refresh-toggle" checked>
      </div>
    </div>

    <!-- Data Table -->
    <div class="table-container">
      <table>
        <thead>
          <tr>
            <th>Priority</th>
            <th>Job ID</th>
            <th>Type</th>
            <th>Payload</th>
            <th>Status</th>
            <th>Attempts</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody id="jobs-table-body">
          <tr>
            <td colspan="7" style="text-align:center; color:var(--text-muted); padding:3rem;">Loading jobs from TaskMesh engine...</td>
          </tr>
        </tbody>
      </table>
    </div>
  </main>

  <!-- Submit Job Modal -->
  <div class="modal-overlay" id="submit-modal">
    <div class="modal-box">
      <div class="modal-header">
        <div class="modal-title">Schedule New Job</div>
        <button class="modal-close" onclick="closeSubmitModal()">&times;</button>
      </div>
      <form id="job-form" onsubmit="submitJob(event)">
        <div class="form-group">
          <label>Job Identifier (Optional)</label>
          <input type="text" id="form-job-id" placeholder="e.g. email-dispatch-42 (auto-generated if empty)">
        </div>
        <div class="form-group">
          <label>Job Type *</label>
          <select id="form-job-type" required onchange="onJobTypeChanged()">
            <option value="email">email (Simulated Email Dispatch)</option>
            <option value="data_sync">data_sync (Database Replication)</option>
            <option value="flaky">flaky (Transient Failure - Recovers on Retry)</option>
            <option value="failing">failing (Fatal Error Simulation - Test Retries)</option>
            <option value="compute">compute (Multi-Stage Matrix Computation)</option>
            <option value="custom">custom (Generic Task)</option>
          </select>
        </div>
        <div class="form-group">
          <label>Scheduling Priority *</label>
          <select id="form-priority" required>
            <option value="50">Normal (50)</option>
            <option value="100">High (100)</option>
            <option value="200">Critical (200)</option>
            <option value="10">Low (10)</option>
          </select>
        </div>
        <div class="form-group">
          <label>Max Retry Attempts</label>
          <input type="number" id="form-max-attempts" value="3" min="1" max="10">
        </div>
        <div class="form-group">
          <label>Payload (JSON or Text)</label>
          <textarea id="form-payload">{"recipient": "user@example.com", "action": "deliver"}</textarea>
        </div>
        <div style="display:flex; justify-content:flex-end; gap:0.75rem; margin-top:1.5rem;">
          <button type="button" class="btn btn-secondary" onclick="closeSubmitModal()">Cancel</button>
          <button type="submit" class="btn btn-primary">Schedule Job</button>
        </div>
      </form>
    </div>
  </div>

  <!-- Job Details Modal -->
  <div class="modal-overlay" id="details-modal">
    <div class="modal-box">
      <div class="modal-header">
        <div>
          <div class="modal-title" id="details-title">Job Details</div>
          <div style="font-family:var(--font-mono); font-size:0.8rem; color:var(--text-muted); margin-top:0.2rem;" id="details-subtitle"></div>
        </div>
        <button class="modal-close" onclick="closeDetailsModal()">&times;</button>
      </div>

      <div id="details-error-container" style="display:none;" class="error-banner">
        <strong>Error Details:</strong>
        <div id="details-error-text" style="margin-top:0.35rem; font-family:var(--font-mono); font-size:0.8rem;"></div>
      </div>

      <div class="meta-grid">
        <div class="meta-item">
          <span>STATUS</span>
          <span id="details-status"></span>
        </div>
        <div class="meta-item">
          <span>PRIORITY</span>
          <span id="details-priority"></span>
        </div>
        <div class="meta-item">
          <span>ATTEMPTS</span>
          <span id="details-attempts"></span>
        </div>
        <div class="meta-item">
          <span>SCHEDULED AT</span>
          <span id="details-scheduled"></span>
        </div>
      </div>

      <div class="form-group">
        <div style="display:flex; justify-content:space-between; align-items:center;">
          <label>PAYLOAD</label>
          <button type="button" class="btn btn-secondary btn-xs" onclick="copyPayload()">Copy</button>
        </div>
        <pre class="payload-pre" id="details-payload"></pre>
      </div>

      <div style="display:flex; justify-content:space-between; align-items:center; margin-top:1.5rem;">
        <div id="details-actions" style="display:flex; gap:0.5rem;"></div>
        <button type="button" class="btn btn-secondary" onclick="closeDetailsModal()">Close</button>
      </div>
    </div>
  </div>

  <!-- API Key Modal -->
  <div class="modal-overlay" id="apikey-modal">
    <div class="modal-box">
      <div class="modal-header">
        <div class="modal-title">API Authentication Key</div>
        <button class="modal-close" onclick="closeApiKeyModal()">&times;</button>
      </div>
      <p style="font-size:0.85rem; color:var(--text-muted); margin-bottom:1.25rem;">
        If TaskMesh was started with <code>TASKMESH_API_KEY</code>, provide your secret token below to authenticate dashboard requests.
      </p>
      <div class="form-group">
        <label>X-API-Key / Bearer Token</label>
        <input type="password" id="input-api-key" placeholder="Enter API key...">
      </div>
      <div class="form-group" style="display:flex; align-items:center; gap:0.5rem; margin-top:0.75rem;">
        <input type="checkbox" id="check-remember-key" style="width:auto; cursor:pointer;">
        <label for="check-remember-key" style="margin:0; font-size:0.8rem; color:var(--text-muted); cursor:pointer;">
          Remember across browser restarts (Local Storage). Unchecked = session only.
        </label>
      </div>
      <div style="display:flex; justify-content:space-between; margin-top:1.5rem;">
        <button type="button" class="btn btn-danger btn-xs" onclick="clearApiKey()">Clear Saved Key</button>
        <div style="display:flex; gap:0.5rem;">
          <button type="button" class="btn btn-secondary" onclick="closeApiKeyModal()">Cancel</button>
          <button type="button" class="btn btn-primary" onclick="saveApiKey()">Save Key</button>
        </div>
      </div>
    </div>
  </div>

  <!-- Toast Container -->
  <div id="toast-container"></div>

  <script>
    let currentFilter = null;
    let allJobs = [];
    let currentDetailsJob = null;

    function getStoredApiKey() {
      return sessionStorage.getItem('taskmesh_api_key') || localStorage.getItem('taskmesh_api_key') || '';
    }

    function isKeyInLocalStorage() {
      return !!localStorage.getItem('taskmesh_api_key');
    }

    function getHeaders() {
      const h = { 'Content-Type': 'application/json' };
      const key = getStoredApiKey();
      if (key) {
        h['X-API-Key'] = key;
        h['Authorization'] = 'Bearer ' + key;
      }
      return h;
    }

    function showToast(msg, type = 'info') {
      const container = document.getElementById('toast-container');
      const toast = document.createElement('div');
      toast.className = `toast toast-${type}`;
      toast.innerText = msg;
      container.appendChild(toast);
      setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transition = 'opacity 0.3s';
        setTimeout(() => toast.remove(), 300);
      }, 3500);
    }

    function updateApiKeyIndicator() {
      const key = getStoredApiKey();
      const btnText = document.getElementById('api-key-status-text');
      if (key) {
        const isPersistent = isKeyInLocalStorage();
        btnText.innerHTML = `<span style="color:#34d399;">●</span> ${isPersistent ? 'Key (Persistent)' : 'Key (Session)'}`;
      } else {
        btnText.innerText = 'Set API Key';
      }
    }

    function openApiKeyModal() {
      document.getElementById('input-api-key').value = getStoredApiKey();
      document.getElementById('check-remember-key').checked = isKeyInLocalStorage();
      document.getElementById('apikey-modal').classList.add('active');
    }
    function closeApiKeyModal() { document.getElementById('apikey-modal').classList.remove('active'); }

    function saveApiKey() {
      const key = document.getElementById('input-api-key').value.trim();
      const remember = document.getElementById('check-remember-key').checked;
      if (key) {
        if (remember) {
          localStorage.setItem('taskmesh_api_key', key);
          sessionStorage.removeItem('taskmesh_api_key');
          showToast('API Key saved to local storage', 'success');
        } else {
          sessionStorage.setItem('taskmesh_api_key', key);
          localStorage.removeItem('taskmesh_api_key');
          showToast('API Key saved for this session only', 'success');
        }
      } else {
        localStorage.removeItem('taskmesh_api_key');
        sessionStorage.removeItem('taskmesh_api_key');
        showToast('API Key cleared', 'info');
      }
      closeApiKeyModal();
      updateApiKeyIndicator();
      fetchData();
    }

    function clearApiKey() {
      localStorage.removeItem('taskmesh_api_key');
      sessionStorage.removeItem('taskmesh_api_key');
      document.getElementById('input-api-key').value = '';
      document.getElementById('check-remember-key').checked = false;
      closeApiKeyModal();
      updateApiKeyIndicator();
      showToast('API Key removed', 'info');
      fetchData();
    }

    function openSubmitModal() { document.getElementById('submit-modal').classList.add('active'); }
    function closeSubmitModal() { document.getElementById('submit-modal').classList.remove('active'); }

    function closeDetailsModal() { document.getElementById('details-modal').classList.remove('active'); }

    function onJobTypeChanged() {
      const t = document.getElementById('form-job-type').value;
      const p = document.getElementById('form-payload');
      if (t === 'email') p.value = '{"recipient": "user@example.com", "subject": "TaskMesh Alert"}';
      else if (t === 'data_sync') p.value = '{"source": "prod_db", "target": "analytics_dw", "records": 1000}';
      else if (t === 'flaky') p.value = '{"simulate_flaky": true, "note": "Recovers automatically on retry attempt"}';
      else if (t === 'failing') p.value = '{"simulate_error": true, "target": "unreachable_host"}';
      else if (t === 'compute') p.value = '{"matrix_dim": 512, "iterations": 1000}';
      else p.value = '{"action": "custom_task"}';
    }

    function setFilter(status, el) {
      currentFilter = status;
      document.querySelectorAll('.filter-pill').forEach(b => b.classList.remove('active'));
      el.classList.add('active');
      filterAndRender();
    }

    async function fetchData() {
      const icon = document.getElementById('refresh-icon');
      if (icon) icon.style.animation = 'pulse 0.5s';

      try {
        const [mRes, jRes] = await Promise.all([
          fetch('/api/v1/metrics', { headers: getHeaders() }),
          fetch('/api/v1/jobs?limit=200', { headers: getHeaders() })
        ]);

        if (mRes.ok) {
          const m = await mRes.json();
          document.getElementById('val-submitted').innerText = m.submitted;
          document.getElementById('val-queued').innerText = m.queued;
          document.getElementById('val-running').innerText = m.running;
          document.getElementById('val-completed').innerText = m.completed;
          document.getElementById('val-failed').innerText = m.failed;
          document.getElementById('val-cancelled').innerText = m.cancelled;
        }

        if (jRes.ok) {
          const data = await jRes.json();
          allJobs = data.jobs || [];
          filterAndRender();
        }
      } catch (err) {
        console.error('TaskMesh poll error:', err);
      } finally {
        if (icon) icon.style.animation = '';
      }
    }

    function filterAndRender() {
      const query = (document.getElementById('search-input').value || '').toLowerCase().trim();
      let filtered = allJobs;

      if (currentFilter) {
        filtered = filtered.filter(j => j.status === currentFilter);
      }

      if (query) {
        filtered = filtered.filter(j => 
          (j.id && j.id.toLowerCase().includes(query)) ||
          (j.type && j.type.toLowerCase().includes(query)) ||
          (j.payload && j.payload.toLowerCase().includes(query)) ||
          (j.error_details && j.error_details.toLowerCase().includes(query))
        );
      }

      renderJobs(filtered);
    }

    function renderJobs(jobs) {
      const tbody = document.getElementById('jobs-table-body');
      if (jobs.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" style="text-align:center; color:var(--text-muted); padding:3rem;">No jobs match your search or filter criteria.</td></tr>';
        return;
      }

      tbody.innerHTML = jobs.map(j => {
        const canCancel = (j.status === 'Queued' || j.status === 'Running');
        const canRetry = (j.status === 'Failed' || j.status === 'Cancelled');
        const canDelete = (j.status === 'Completed' || j.status === 'Failed' || j.status === 'Cancelled');
        const prioName = j.priority_name ? j.priority_name.split('(')[0] : 'Normal';
        const hasErr = Boolean(j.error_details);

        return `
          <tr>
            <td><span class="priority-badge priority-${prioName}">${j.priority_name}</span></td>
            <td>
              <strong style="cursor:pointer; color:#60a5fa;" onclick="openDetails('${escapeHtml(j.id)}')">${escapeHtml(j.id)}</strong>
            </td>
            <td>${escapeHtml(j.type)}</td>
            <td>
              <span class="code-text" title="${escapeHtml(j.payload)}">${escapeHtml(j.payload)}</span>
              ${hasErr ? `<span class="error-snippet" title="${escapeHtml(j.error_details)}">⚠ ${escapeHtml(j.error_details)}</span>` : ''}
            </td>
            <td><span class="status-badge status-${j.status}">${j.status}</span></td>
            <td>${j.attempt_count} / ${j.max_attempts}</td>
            <td>
              <div style="display:flex; gap:0.4rem; align-items:center;">
                <button class="btn btn-secondary btn-xs" onclick="openDetails('${escapeHtml(j.id)}')">Inspect</button>
                ${canCancel ? `<button class="btn btn-warning btn-xs" onclick="cancelJob('${escapeHtml(j.id)}')">Cancel</button>` : ''}
                ${canRetry ? `<button class="btn btn-primary btn-xs" onclick="retryJob('${escapeHtml(j.id)}')">Retry</button>` : ''}
                ${canDelete ? `<button class="btn btn-danger btn-xs" onclick="deleteJob('${escapeHtml(j.id)}')">Delete</button>` : ''}
              </div>
            </td>
          </tr>
        `;
      }).join('');
    }

    function openDetails(id) {
      const job = allJobs.find(j => j.id === id);
      if (!job) return;
      currentDetailsJob = job;

      document.getElementById('details-title').innerText = job.type + ' Task';
      document.getElementById('details-subtitle').innerText = 'ID: ' + job.id;

      const prioName = job.priority_name ? job.priority_name.split('(')[0] : 'Normal';
      document.getElementById('details-priority').innerHTML = `<span class="priority-badge priority-${prioName}">${job.priority_name}</span>`;
      document.getElementById('details-status').innerHTML = `<span class="status-badge status-${job.status}">${job.status}</span>`;
      document.getElementById('details-attempts').innerText = `${job.attempt_count} of ${job.max_attempts}`;
      document.getElementById('details-scheduled').innerText = new Date(job.created_at_epoch_ms || Date.now()).toLocaleTimeString();

      const errContainer = document.getElementById('details-error-container');
      if (job.error_details) {
        errContainer.style.display = 'block';
        document.getElementById('details-error-text').innerText = job.error_details;
      } else {
        errContainer.style.display = 'none';
      }

      let formattedPayload = job.payload;
      try {
        formattedPayload = JSON.stringify(JSON.parse(job.payload), null, 2);
      } catch (_) {}
      document.getElementById('details-payload').innerText = formattedPayload;

      // Modal action buttons
      const actionsDiv = document.getElementById('details-actions');
      actionsDiv.innerHTML = '';
      if (job.status === 'Queued' || job.status === 'Running') {
        const btn = document.createElement('button');
        btn.className = 'btn btn-warning btn-xs';
        btn.innerText = 'Cancel Job';
        btn.onclick = () => { cancelJob(job.id); closeDetailsModal(); };
        actionsDiv.appendChild(btn);
      }
      if (job.status === 'Failed' || job.status === 'Cancelled') {
        const btn = document.createElement('button');
        btn.className = 'btn btn-primary btn-xs';
        btn.innerText = 'Retry Job';
        btn.onclick = () => { retryJob(job.id); closeDetailsModal(); };
        actionsDiv.appendChild(btn);
      }
      if (job.status === 'Completed' || job.status === 'Failed' || job.status === 'Cancelled') {
        const btn = document.createElement('button');
        btn.className = 'btn btn-danger btn-xs';
        btn.innerText = 'Delete Record';
        btn.onclick = () => { deleteJob(job.id); closeDetailsModal(); };
        actionsDiv.appendChild(btn);
      }

      document.getElementById('details-modal').classList.add('active');
    }

    function copyPayload() {
      if (currentDetailsJob && currentDetailsJob.payload) {
        navigator.clipboard.writeText(currentDetailsJob.payload).then(() => {
          showToast('Payload copied to clipboard', 'success');
        });
      }
    }

    async function submitJob(e) {
      e.preventDefault();
      const id = document.getElementById('form-job-id').value.trim();
      const type = document.getElementById('form-job-type').value;
      const priority = parseInt(document.getElementById('form-priority').value, 10);
      const max_attempts = parseInt(document.getElementById('form-max-attempts').value, 10);
      const payload = document.getElementById('form-payload').value.trim();

      const body = { type, priority, max_attempts, payload };
      if (id) body.id = id;

      try {
        const res = await fetch('/api/v1/jobs', {
          method: 'POST',
          headers: getHeaders(),
          body: JSON.stringify(body)
        });
        const resData = await res.json();
        if (res.ok) {
          closeSubmitModal();
          showToast('Job scheduled successfully', 'success');
          fetchData();
        } else {
          showToast('Submit error: ' + (resData.error || 'Failed'), 'error');
        }
      } catch (err) {
        showToast('Request failed: ' + err.message, 'error');
      }
    }

    async function cancelJob(id) {
      try {
        const res = await fetch(`/api/v1/jobs/${encodeURIComponent(id)}/cancel`, {
          method: 'POST',
          headers: getHeaders()
        });
        const resData = await res.json();
        if (res.ok) {
          showToast(`Job ${id} cancelled`, 'info');
          fetchData();
        } else {
          showToast('Cancel error: ' + (resData.error || 'Failed'), 'error');
        }
      } catch (err) {
        showToast('Request failed: ' + err.message, 'error');
      }
    }

    async function retryJob(id) {
      try {
        const res = await fetch(`/api/v1/jobs/${encodeURIComponent(id)}/retry`, {
          method: 'POST',
          headers: getHeaders()
        });
        const resData = await res.json();
        if (res.ok) {
          showToast(`Job ${id} re-queued for retry`, 'success');
          fetchData();
        } else {
          showToast('Retry error: ' + (resData.error || 'Failed'), 'error');
        }
      } catch (err) {
        showToast('Request failed: ' + err.message, 'error');
      }
    }

    async function deleteJob(id) {
      try {
        const res = await fetch(`/api/v1/jobs/${encodeURIComponent(id)}`, {
          method: 'DELETE',
          headers: getHeaders()
        });
        const resData = await res.json();
        if (res.ok) {
          showToast(`Job ${id} deleted`, 'info');
          fetchData();
        } else {
          showToast('Delete error: ' + (resData.error || 'Failed'), 'error');
        }
      } catch (err) {
        showToast('Request failed: ' + err.message, 'error');
      }
    }

    function escapeHtml(str) {
      if (!str) return '';
      return String(str).replace(/[&<>"']/g, s => ({ '&':'&amp;', '<':'&lt;', '>':'&gt;', '"':'&quot;', "'":'&#39;' })[s]);
    }

    // Auto-refresh interval
    setInterval(() => {
      if (document.getElementById('auto-refresh-toggle').checked) {
        fetchData();
      }
    }, 2000);

    updateApiKeyIndicator();
    fetchData();
  </script>
</body>
</html>)rawhtml";
}

} // namespace taskmesh
