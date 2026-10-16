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
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=JetBrains+Mono:wght@400;500;600&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg-dark: #080c14;
      --bg-surface: #0f172a;
      --bg-card: rgba(17, 24, 39, 0.85);
      --bg-card-hover: rgba(30, 41, 59, 0.9);
      --bg-input: #151e32;
      --border-color: rgba(148, 163, 184, 0.14);
      --border-focus: #3b82f6;
      --text-main: #f8fafc;
      --text-secondary: #cbd5e1;
      --text-muted: #94a3b8;
      --primary: #3b82f6;
      --primary-hover: #2563eb;
      --primary-glow: rgba(59, 130, 246, 0.35);
      --success: #10b981;
      --success-glow: rgba(16, 185, 129, 0.3);
      --warning: #f59e0b;
      --warning-glow: rgba(245, 158, 11, 0.3);
      --danger: #ef4444;
      --danger-hover: #dc2626;
      --danger-glow: rgba(239, 68, 68, 0.3);
      --purple: #8b5cf6;
      --cancelled: #64748b;
      --font-sans: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      --font-mono: 'JetBrains Mono', 'Fira Code', Menlo, Consolas, monospace;
      --radius-sm: 6px;
      --radius-md: 10px;
      --radius-lg: 16px;
      --transition-fast: 0.15s ease;
      --transition-normal: 0.25s ease;
    }

    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }

    html, body {
      min-height: 100%;
      overflow-x: hidden;
      width: 100%;
    }

    body {
      background-color: var(--bg-dark);
      background-image:
        radial-gradient(circle at 10% 10%, rgba(59, 130, 246, 0.08) 0%, transparent 40%),
        radial-gradient(circle at 90% 90%, rgba(139, 92, 246, 0.07) 0%, transparent 40%);
      background-attachment: fixed;
      color: var(--text-main);
      font-family: var(--font-sans);
      font-size: 14px;
      line-height: 1.5;
      display: flex;
      flex-direction: column;
      -webkit-font-smoothing: antialiased;
      -moz-osx-font-smoothing: grayscale;
    }

    /* Accessibility visible focus */
    :focus-visible {
      outline: 2px solid var(--primary);
      outline-offset: 2px;
    }

    /* Header */
    header {
      background: rgba(8, 12, 20, 0.92);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border-bottom: 1px solid var(--border-color);
      position: sticky;
      top: 0;
      z-index: 50;
      padding: 0.75rem 1.5rem;
      display: flex;
      align-items: center;
      justify-content: space-between;
      width: 100%;
    }

    .brand {
      display: flex;
      align-items: center;
      gap: 0.75rem;
      text-decoration: none;
      color: inherit;
    }

    .brand-icon {
      width: 38px;
      height: 38px;
      background: linear-gradient(135deg, #2563eb, #7c3aed);
      border-radius: var(--radius-md);
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 0 16px var(--primary-glow);
      flex-shrink: 0;
    }

    .brand-icon svg {
      width: 20px;
      height: 20px;
      fill: white;
    }

    .brand-title-wrap {
      display: flex;
      align-items: center;
      gap: 0.6rem;
      flex-wrap: wrap;
    }

    .brand-title {
      font-size: 1.15rem;
      font-weight: 700;
      letter-spacing: -0.02em;
      color: var(--text-main);
    }

    .brand-badge {
      font-size: 0.68rem;
      font-weight: 600;
      padding: 0.15rem 0.5rem;
      border-radius: 9999px;
      background: rgba(59, 130, 246, 0.15);
      border: 1px solid rgba(59, 130, 246, 0.3);
      color: #93c5fd;
      letter-spacing: 0.04em;
      text-transform: uppercase;
      font-family: var(--font-mono);
      white-space: nowrap;
    }

    .header-right {
      display: flex;
      align-items: center;
      gap: 0.75rem;
    }

    .status-indicator {
      display: inline-flex;
      align-items: center;
      gap: 0.45rem;
      font-size: 0.8rem;
      font-weight: 500;
      color: var(--text-secondary);
      background: rgba(255, 255, 255, 0.04);
      padding: 0.35rem 0.75rem;
      border-radius: var(--radius-sm);
      border: 1px solid var(--border-color);
      white-space: nowrap;
    }

    .pulse-dot {
      width: 8px;
      height: 8px;
      background: var(--success);
      border-radius: 50%;
      box-shadow: 0 0 8px var(--success-glow);
      animation: pulseDot 2s infinite ease-in-out;
      flex-shrink: 0;
    }

    @keyframes pulseDot {
      0%, 100% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.4; transform: scale(0.85); }
    }

    .header-actions {
      display: flex;
      align-items: center;
      gap: 0.6rem;
    }

    /* Mobile hamburger menu toggle */
    .mobile-menu-btn {
      display: none;
      align-items: center;
      justify-content: center;
      width: 44px;
      height: 44px;
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--border-color);
      border-radius: var(--radius-sm);
      color: var(--text-main);
      cursor: pointer;
    }

    .mobile-menu-btn svg {
      width: 20px;
      height: 20px;
      stroke: currentColor;
    }

    /* Buttons */
    .btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 0.45rem;
      padding: 0.45rem 0.85rem;
      border-radius: var(--radius-sm);
      font-size: 0.825rem;
      font-weight: 600;
      cursor: pointer;
      transition: all var(--transition-fast);
      border: 1px solid transparent;
      font-family: inherit;
      white-space: nowrap;
      user-select: none;
      text-decoration: none;
    }

    .btn:disabled {
      opacity: 0.55;
      cursor: not-allowed;
      transform: none !important;
    }

    .btn-primary {
      background: var(--primary);
      color: white;
      box-shadow: 0 0 12px var(--primary-glow);
    }
    .btn-primary:hover:not(:disabled) {
      background: var(--primary-hover);
      transform: translateY(-1px);
    }

    .btn-secondary {
      background: rgba(255, 255, 255, 0.05);
      color: var(--text-main);
      border-color: var(--border-color);
    }
    .btn-secondary:hover:not(:disabled) {
      background: rgba(255, 255, 255, 0.09);
      border-color: rgba(255, 255, 255, 0.22);
    }

    .btn-danger {
      background: rgba(239, 68, 68, 0.15);
      color: #fca5a5;
      border-color: rgba(239, 68, 68, 0.35);
    }
    .btn-danger:hover:not(:disabled) {
      background: rgba(239, 68, 68, 0.25);
      border-color: var(--danger);
      color: #fee2e2;
    }

    .btn-danger-solid {
      background: var(--danger);
      color: white;
      box-shadow: 0 0 12px var(--danger-glow);
    }
    .btn-danger-solid:hover:not(:disabled) {
      background: var(--danger-hover);
    }

    .btn-warning {
      background: rgba(245, 158, 11, 0.15);
      color: #fcd34d;
      border-color: rgba(245, 158, 11, 0.35);
    }
    .btn-warning:hover:not(:disabled) {
      background: rgba(245, 158, 11, 0.25);
      border-color: var(--warning);
    }

    .btn-xs {
      padding: 0.3rem 0.6rem;
      font-size: 0.75rem;
      border-radius: 4px;
    }

    /* Main Container */
    main {
      flex: 1;
      padding: 1.5rem;
      max-width: 1440px;
      margin: 0 auto;
      width: 100%;
    }

    /* Metrics Grid */
    .metrics-grid {
      display: grid;
      grid-template-columns: repeat(6, 1fr);
      gap: 1rem;
      margin-bottom: 1.75rem;
    }

    .metric-card {
      background: var(--bg-card);
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      border: 1px solid var(--border-color);
      border-radius: var(--radius-md);
      padding: 1rem 1.15rem;
      transition: transform var(--transition-fast), background-color var(--transition-fast);
      position: relative;
      overflow: hidden;
    }

    .metric-card:hover {
      background: var(--bg-card-hover);
      transform: translateY(-2px);
    }

    .metric-card::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      height: 3px;
    }

    .card-submitted::before { background: var(--primary); }
    .card-queued::before { background: var(--warning); }
    .card-running::before { background: var(--purple); }
    .card-completed::before { background: var(--success); }
    .card-failed::before { background: var(--danger); }
    .card-cancelled::before { background: var(--cancelled); }

    .metric-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 0.4rem;
    }

    .metric-label {
      font-size: 0.72rem;
      font-weight: 600;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.06em;
    }

    .metric-value {
      font-size: 1.65rem;
      font-weight: 700;
      font-family: var(--font-mono);
      color: var(--text-main);
      line-height: 1.2;
    }

    /* Toolbar */
    .toolbar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 1.25rem;
      flex-wrap: wrap;
      gap: 0.85rem;
    }

    .search-wrap {
      position: relative;
      flex: 1;
      max-width: 380px;
      min-width: 220px;
    }

    .search-icon {
      position: absolute;
      left: 0.75rem;
      top: 50%;
      transform: translateY(-50%);
      color: var(--text-muted);
      pointer-events: none;
      width: 16px;
      height: 16px;
    }

    .search-input {
      width: 100%;
      background: var(--bg-input);
      border: 1px solid var(--border-color);
      border-radius: var(--radius-sm);
      padding: 0.5rem 2.2rem 0.5rem 2.25rem;
      color: var(--text-main);
      font-family: inherit;
      font-size: 0.85rem;
      transition: border-color var(--transition-fast), box-shadow var(--transition-fast);
    }

    .search-input:focus {
      outline: none;
      border-color: var(--primary);
      box-shadow: 0 0 10px var(--primary-glow);
    }

    .search-clear-btn {
      position: absolute;
      right: 0.6rem;
      top: 50%;
      transform: translateY(-50%);
      background: transparent;
      border: none;
      color: var(--text-muted);
      cursor: pointer;
      display: none;
      align-items: center;
      justify-content: center;
      padding: 0.2rem;
      border-radius: 50%;
    }
    .search-clear-btn:hover {
      color: var(--text-main);
    }

    .filter-group {
      display: flex;
      align-items: center;
      gap: 0.35rem;
      flex-wrap: wrap;
    }

    .filter-pill {
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--border-color);
      color: var(--text-secondary);
      padding: 0.35rem 0.75rem;
      border-radius: 9999px;
      font-size: 0.78rem;
      font-weight: 500;
      cursor: pointer;
      transition: all var(--transition-fast);
      user-select: none;
    }

    .filter-pill:hover {
      background: rgba(255, 255, 255, 0.08);
      color: var(--text-main);
    }

    .filter-pill.active {
      background: var(--primary);
      border-color: var(--primary);
      color: white;
      font-weight: 600;
    }

    .auto-refresh-wrap {
      display: flex;
      align-items: center;
      gap: 0.6rem;
      margin-left: auto;
      background: rgba(255, 255, 255, 0.03);
      padding: 0.35rem 0.65rem;
      border-radius: var(--radius-sm);
      border: 1px solid var(--border-color);
    }

    .auto-refresh-wrap label {
      font-size: 0.78rem;
      color: var(--text-muted);
      cursor: pointer;
      user-select: none;
      display: flex;
      align-items: center;
      gap: 0.45rem;
    }

    .auto-refresh-wrap input[type="checkbox"] {
      cursor: pointer;
      accent-color: var(--primary);
      width: 15px;
      height: 15px;
    }

    /* Table Container */
    .table-container {
      background: var(--bg-card);
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      border: 1px solid var(--border-color);
      border-radius: var(--radius-md);
      overflow-x: auto;
      width: 100%;
    }

    table {
      width: 100%;
      border-collapse: collapse;
      text-align: left;
      font-size: 0.85rem;
    }

    th {
      background: rgba(255, 255, 255, 0.02);
      color: var(--text-muted);
      font-weight: 600;
      font-size: 0.72rem;
      text-transform: uppercase;
      letter-spacing: 0.06em;
      padding: 0.85rem 1.15rem;
      border-bottom: 1px solid var(--border-color);
      white-space: nowrap;
    }

    td {
      padding: 0.85rem 1.15rem;
      border-bottom: 1px solid rgba(255, 255, 255, 0.04);
      vertical-align: middle;
    }

    tbody tr:hover td {
      background: rgba(255, 255, 255, 0.02);
    }

    /* Badges */
    .priority-badge {
      display: inline-block;
      font-size: 0.72rem;
      font-weight: 600;
      padding: 0.15rem 0.5rem;
      border-radius: 4px;
      font-family: var(--font-mono);
      white-space: nowrap;
    }

    .priority-Low { background: rgba(100, 116, 139, 0.16); color: #94a3b8; border: 1px solid rgba(100, 116, 139, 0.25); }
    .priority-Normal { background: rgba(59, 130, 246, 0.14); color: #93c5fd; border: 1px solid rgba(59, 130, 246, 0.25); }
    .priority-High { background: rgba(245, 158, 11, 0.14); color: #fcd34d; border: 1px solid rgba(245, 158, 11, 0.25); }
    .priority-Critical { background: rgba(236, 72, 153, 0.14); color: #f472b6; border: 1px solid rgba(236, 72, 153, 0.25); }

    .status-badge {
      display: inline-flex;
      align-items: center;
      gap: 0.35rem;
      font-size: 0.72rem;
      font-weight: 600;
      padding: 0.15rem 0.55rem;
      border-radius: 9999px;
      white-space: nowrap;
    }

    .status-Queued { background: rgba(245, 158, 11, 0.14); color: #fbbf24; border: 1px solid rgba(245, 158, 11, 0.3); }
    .status-Running { background: rgba(139, 92, 246, 0.14); color: #c084fc; border: 1px solid rgba(139, 92, 246, 0.3); }
    .status-Completed { background: rgba(16, 185, 129, 0.14); color: #34d399; border: 1px solid rgba(16, 185, 129, 0.3); }
    .status-Failed { background: rgba(239, 68, 68, 0.14); color: #f87171; border: 1px solid rgba(239, 68, 68, 0.3); }
    .status-Cancelled { background: rgba(100, 116, 139, 0.14); color: #94a3b8; border: 1px solid rgba(100, 116, 139, 0.3); }

    .code-text {
      font-family: var(--font-mono);
      font-size: 0.78rem;
      background: rgba(0, 0, 0, 0.35);
      padding: 0.18rem 0.45rem;
      border-radius: 4px;
      max-width: 220px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
      display: inline-block;
      vertical-align: middle;
      border: 1px solid rgba(255, 255, 255, 0.05);
      color: #93c5fd;
    }

    .error-snippet {
      display: block;
      color: #fca5a5;
      font-size: 0.72rem;
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
      background: rgba(0, 0, 0, 0.78);
      backdrop-filter: blur(8px);
      -webkit-backdrop-filter: blur(8px);
      display: none;
      align-items: center;
      justify-content: center;
      z-index: 100;
      padding: 1rem;
    }

    .modal-overlay.active {
      display: flex;
    }

    .modal-box {
      background: #0f172a;
      border: 1px solid var(--border-color);
      border-radius: var(--radius-lg);
      max-width: 580px;
      width: 100%;
      padding: 1.75rem;
      box-shadow: 0 24px 50px rgba(0, 0, 0, 0.7);
      max-height: 90vh;
      overflow-y: auto;
      position: relative;
    }

    .modal-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 1.25rem;
      gap: 1rem;
    }

    .modal-title {
      font-size: 1.15rem;
      font-weight: 700;
      color: var(--text-main);
    }

    .modal-close {
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--border-color);
      color: var(--text-muted);
      cursor: pointer;
      width: 36px;
      height: 36px;
      border-radius: var(--radius-sm);
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 1.25rem;
      line-height: 1;
      transition: all var(--transition-fast);
      flex-shrink: 0;
    }
    .modal-close:hover {
      background: rgba(255, 255, 255, 0.1);
      color: var(--text-main);
    }

    .form-group {
      margin-bottom: 1rem;
      display: flex;
      flex-direction: column;
      gap: 0.35rem;
    }

    label {
      font-size: 0.78rem;
      font-weight: 600;
      color: var(--text-secondary);
      letter-spacing: 0.02em;
    }

    input, select, textarea {
      background: var(--bg-input);
      border: 1px solid var(--border-color);
      border-radius: var(--radius-sm);
      padding: 0.55rem 0.8rem;
      color: var(--text-main);
      font-family: inherit;
      font-size: 0.85rem;
      transition: border-color var(--transition-fast), box-shadow var(--transition-fast);
      width: 100%;
    }

    input:focus, select:focus, textarea:focus {
      outline: none;
      border-color: var(--primary);
      box-shadow: 0 0 10px var(--primary-glow);
    }

    textarea {
      resize: vertical;
      min-height: 85px;
      font-family: var(--font-mono);
      font-size: 0.82rem;
      line-height: 1.4;
    }

    .error-banner {
      background: rgba(239, 68, 68, 0.12);
      border: 1px solid rgba(239, 68, 68, 0.35);
      border-radius: var(--radius-sm);
      padding: 0.75rem 1rem;
      margin-bottom: 1rem;
      color: #fca5a5;
      font-size: 0.82rem;
      line-height: 1.4;
      word-break: break-word;
    }

    .meta-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 0.75rem;
      margin-bottom: 1.25rem;
      background: rgba(0, 0, 0, 0.25);
      padding: 0.85rem 1rem;
      border-radius: var(--radius-sm);
      border: 1px solid var(--border-color);
    }

    .meta-item {
      display: flex;
      flex-direction: column;
      gap: 0.2rem;
    }
    .meta-item span:first-child {
      font-size: 0.7rem;
      color: var(--text-muted);
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }
    .meta-item span:last-child {
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text-main);
    }

    .payload-pre {
      background: #070a10;
      border: 1px solid var(--border-color);
      border-radius: var(--radius-sm);
      padding: 0.85rem;
      font-family: var(--font-mono);
      font-size: 0.8rem;
      overflow-x: auto;
      max-height: 220px;
      color: #93c5fd;
      white-space: pre-wrap;
      word-break: break-all;
    }

    /* Toasts */
    #toast-container {
      position: fixed;
      bottom: 1.25rem;
      right: 1.25rem;
      z-index: 200;
      display: flex;
      flex-direction: column;
      gap: 0.5rem;
      max-width: 380px;
      width: calc(100% - 2.5rem);
      pointer-events: none;
    }

    .toast {
      background: #1e293b;
      border: 1px solid var(--border-color);
      color: var(--text-main);
      padding: 0.75rem 1rem;
      border-radius: var(--radius-sm);
      box-shadow: 0 12px 30px rgba(0, 0, 0, 0.6);
      font-size: 0.825rem;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 0.6rem;
      animation: toastSlideIn 0.25s ease-out;
      pointer-events: auto;
    }

    .toast-success { border-left: 4px solid var(--success); }
    .toast-error { border-left: 4px solid var(--danger); }
    .toast-info { border-left: 4px solid var(--primary); }
    .toast-warning { border-left: 4px solid var(--warning); }

    .toast-close {
      background: transparent;
      border: none;
      color: var(--text-muted);
      cursor: pointer;
      font-size: 1.1rem;
      line-height: 1;
      padding: 0 0.2rem;
    }
    .toast-close:hover { color: var(--text-main); }

    @keyframes toastSlideIn {
      from { transform: translateX(100%); opacity: 0; }
      to { transform: translateX(0); opacity: 1; }
    }

    /* Spinner */
    .spinner {
      display: inline-block;
      width: 14px;
      height: 14px;
      border: 2px solid rgba(255, 255, 255, 0.25);
      border-radius: 50%;
      border-top-color: currentColor;
      animation: spin 0.6s linear infinite;
    }
    @keyframes spin {
      to { transform: rotate(360deg); }
    }

    /* Empty state */
    .empty-state {
      text-align: center;
      padding: 3rem 1.5rem;
      color: var(--text-muted);
    }
    .empty-state svg {
      width: 44px;
      height: 44px;
      stroke: var(--text-muted);
      margin-bottom: 0.85rem;
      opacity: 0.6;
    }
    .empty-state-title {
      font-size: 0.95rem;
      font-weight: 600;
      color: var(--text-secondary);
      margin-bottom: 0.35rem;
    }
    .empty-state-desc {
      font-size: 0.8rem;
      margin-bottom: 1rem;
    }

    /* RESPONSIVE DESIGN */
    @media (max-width: 1024px) {
      .metrics-grid {
        grid-template-columns: repeat(3, 1fr);
      }
    }

    @media (max-width: 768px) {
      header {
        padding: 0.75rem 1rem;
      }

      .mobile-menu-btn {
        display: flex;
      }

      /* Collapsible mobile drawer for header actions */
      .header-actions {
        display: none;
        position: absolute;
        top: 100%;
        left: 0;
        right: 0;
        background: rgba(15, 23, 42, 0.98);
        border-bottom: 1px solid var(--border-color);
        padding: 1rem;
        flex-direction: column;
        align-items: stretch;
        gap: 0.6rem;
        box-shadow: 0 10px 30px rgba(0, 0, 0, 0.6);
      }

      .header-actions.mobile-open {
        display: flex;
      }

      .header-actions .btn {
        min-height: 44px;
        width: 100%;
        justify-content: center;
        font-size: 0.9rem;
      }

      main {
        padding: 1rem 0.75rem;
      }

      .metrics-grid {
        grid-template-columns: repeat(2, 1fr);
        gap: 0.65rem;
        margin-bottom: 1.25rem;
      }

      .metric-card {
        padding: 0.85rem 0.95rem;
      }

      .metric-value {
        font-size: 1.45rem;
      }

      .toolbar {
        flex-direction: column;
        align-items: stretch;
        gap: 0.75rem;
      }

      .search-wrap {
        max-width: 100%;
      }

      .search-input {
        min-height: 44px;
        font-size: 0.9rem;
      }

      .filter-group {
        overflow-x: auto;
        padding-bottom: 0.25rem;
        flex-wrap: nowrap;
        -webkit-overflow-scrolling: touch;
      }

      .filter-pill {
        min-height: 38px;
        display: inline-flex;
        align-items: center;
        padding: 0.35rem 0.85rem;
        flex-shrink: 0;
      }

      .auto-refresh-wrap {
        margin-left: 0;
        width: 100%;
        justify-content: space-between;
        min-height: 40px;
      }

      /* Mobile Card layout for jobs */
      .table-container {
        background: transparent;
        border: none;
      }

      table, thead, tbody, th, td, tr {
        display: block;
      }

      thead {
        display: none;
      }

      tbody tr {
        background: var(--bg-card);
        border: 1px solid var(--border-color);
        border-radius: var(--radius-md);
        margin-bottom: 0.85rem;
        padding: 1rem;
        display: flex;
        flex-direction: column;
        gap: 0.5rem;
      }

      tbody td {
        padding: 0.25rem 0;
        border: none;
      }

      .mobile-card-row {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 0.5rem;
        flex-wrap: wrap;
      }

      .mobile-card-id {
        font-family: var(--font-mono);
        font-size: 0.85rem;
        font-weight: 600;
        color: #60a5fa;
        word-break: break-all;
      }

      .mobile-card-actions {
        display: flex;
        gap: 0.5rem;
        margin-top: 0.5rem;
        flex-wrap: wrap;
        width: 100%;
      }

      .mobile-card-actions .btn {
        flex: 1;
        min-height: 44px;
        font-size: 0.825rem;
      }

      .code-text {
        max-width: 100%;
        width: 100%;
      }

      .error-snippet {
        max-width: 100%;
      }

      /* Modals on mobile */
      .modal-box {
        padding: 1.25rem;
        max-height: 94vh;
      }

      .meta-grid {
        grid-template-columns: 1fr;
      }

      .modal-actions-wrap {
        flex-direction: column-reverse;
        gap: 0.65rem;
      }

      .modal-actions-wrap .btn {
        width: 100%;
        min-height: 44px;
      }
    }

    @media (max-width: 360px) {
      .metrics-grid {
        grid-template-columns: 1fr;
      }
      .brand-title {
        font-size: 1rem;
      }
      .brand-badge {
        display: none;
      }
    }

    /* Prefers reduced motion */
    @media (prefers-reduced-motion: reduce) {
      *, *::before, *::after {
        animation-duration: 0.001ms !important;
        animation-iteration-count: 1 !important;
        transition-duration: 0.001ms !important;
      }
    }
  </style>
</head>
<body>

  <!-- Sticky Header -->
  <header>
    <a href="/" class="brand" aria-label="TaskMesh Dashboard Home">
      <div class="brand-icon">
        <svg viewBox="0 0 24 24"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg>
      </div>
      <div class="brand-title-wrap">
        <span class="brand-title">TaskMesh</span>
        <span class="brand-badge">Engine v0.2.0</span>
      </div>
    </a>

    <div class="header-right">
      <div class="status-indicator" id="engine-status-box" title="TaskMesh Engine Status">
        <span class="pulse-dot" id="engine-pulse-dot"></span>
        <span id="engine-status">Engine Healthy</span>
      </div>

      <button class="mobile-menu-btn" id="mobile-menu-toggle" onclick="toggleMobileNav()" aria-label="Toggle navigation menu" aria-expanded="false">
        <svg viewBox="0 0 24 24" fill="none" stroke-width="2"><line x1="3" y1="12" x2="21" y2="12"></line><line x1="3" y1="6" x2="21" y2="6"></line><line x1="3" y1="18" x2="21" y2="18"></line></svg>
      </button>

      <div class="header-actions" id="header-actions">
        <button class="btn btn-secondary" id="btn-api-key" onclick="openApiKeyModal()">
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 2l-2 2m-2-2l2 2m7 0a9 9 0 1 1-18 0 9 9 0 0 1 18 0z"/><path d="M7 11l4.5 4.5"/></svg>
          <span id="api-key-status-text">API Key</span>
        </button>
        <button class="btn btn-secondary" id="btn-refresh" onclick="fetchData()">
          <svg id="refresh-icon" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67"/></svg>
          <span>Refresh</span>
        </button>
        <button class="btn btn-primary" id="btn-submit-job" onclick="openSubmitModal()">
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="5" x2="12" y2="19"></line><line x1="5" y1="12" x2="19" y2="12"></line></svg>
          <span>Schedule Job</span>
        </button>
      </div>
    </div>
  </header>

  <!-- Main Content -->
  <main>
    <!-- Metric Gauges -->
    <section class="metrics-grid" aria-label="Job Statistics">
      <div class="metric-card card-submitted">
        <div class="metric-header">
          <span class="metric-label">Submitted</span>
        </div>
        <div class="metric-value" id="val-submitted">0</div>
      </div>
      <div class="metric-card card-queued">
        <div class="metric-header">
          <span class="metric-label">Queued</span>
        </div>
        <div class="metric-value" id="val-queued">0</div>
      </div>
      <div class="metric-card card-running">
        <div class="metric-header">
          <span class="metric-label">Running</span>
        </div>
        <div class="metric-value" id="val-running">0</div>
      </div>
      <div class="metric-card card-completed">
        <div class="metric-header">
          <span class="metric-label">Completed</span>
        </div>
        <div class="metric-value" id="val-completed">0</div>
      </div>
      <div class="metric-card card-failed">
        <div class="metric-header">
          <span class="metric-label">Failed</span>
        </div>
        <div class="metric-value" id="val-failed">0</div>
      </div>
      <div class="metric-card card-cancelled">
        <div class="metric-header">
          <span class="metric-label">Cancelled</span>
        </div>
        <div class="metric-value" id="val-cancelled">0</div>
      </div>
    </section>

    <!-- Toolbar & Filters -->
    <section class="toolbar" aria-label="Search and Filter Controls">
      <div class="search-wrap">
        <svg class="search-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="8"></circle><line x1="21" y1="21" x2="16.65" y2="16.65"></line></svg>
        <input type="text" id="search-input" class="search-input" placeholder="Search by ID, type, payload..." oninput="onSearchInput()" aria-label="Search jobs">
        <button id="search-clear-btn" class="search-clear-btn" onclick="clearSearch()" aria-label="Clear search input">&times;</button>
      </div>

      <div class="filter-group" role="tablist" aria-label="Filter jobs by status">
        <button class="filter-pill active" id="filter-all" onclick="setFilter(null, this)" role="tab" aria-selected="true">All</button>
        <button class="filter-pill" id="filter-queued" onclick="setFilter('Queued', this)" role="tab" aria-selected="false">Queued</button>
        <button class="filter-pill" id="filter-running" onclick="setFilter('Running', this)" role="tab" aria-selected="false">Running</button>
        <button class="filter-pill" id="filter-completed" onclick="setFilter('Completed', this)" role="tab" aria-selected="false">Completed</button>
        <button class="filter-pill" id="filter-failed" onclick="setFilter('Failed', this)" role="tab" aria-selected="false">Failed</button>
        <button class="filter-pill" id="filter-cancelled" onclick="setFilter('Cancelled', this)" role="tab" aria-selected="false">Cancelled</button>
      </div>

      <div class="auto-refresh-wrap">
        <label for="auto-refresh-toggle">
          <input type="checkbox" id="auto-refresh-toggle" checked>
          <span>Auto-refresh (2s)</span>
        </label>
      </div>
    </section>

    <!-- Data Table / Mobile Card Container -->
    <div class="table-container" id="jobs-container" role="region" aria-label="Jobs List">
      <table id="jobs-table">
        <thead>
          <tr>
            <th scope="col">Priority</th>
            <th scope="col">Job ID</th>
            <th scope="col">Type</th>
            <th scope="col">Payload</th>
            <th scope="col">Status</th>
            <th scope="col">Attempts</th>
            <th scope="col">Actions</th>
          </tr>
        </thead>
        <tbody id="jobs-table-body">
          <tr>
            <td colspan="7" style="text-align:center; color:var(--text-muted); padding:3rem;">
              <div class="spinner" style="margin-right:0.5rem;"></div> Loading jobs from TaskMesh engine...
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  </main>

  <!-- Submit Job Modal -->
  <div class="modal-overlay" id="submit-modal" role="dialog" aria-modal="true" aria-labelledby="submit-modal-title">
    <div class="modal-box">
      <div class="modal-header">
        <div class="modal-title" id="submit-modal-title">Schedule New Job</div>
        <button class="modal-close" onclick="closeSubmitModal()" aria-label="Close schedule job modal">&times;</button>
      </div>
      <form id="job-form" onsubmit="submitJob(event)">
        <div class="form-group">
          <label for="form-job-id">Job Identifier (Optional)</label>
          <input type="text" id="form-job-id" placeholder="e.g. email-dispatch-42 (auto-generated if empty)">
        </div>
        <div class="form-group">
          <label for="form-job-type">Job Type *</label>
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
          <label for="form-priority">Scheduling Priority *</label>
          <select id="form-priority" required>
            <option value="50">Normal (50)</option>
            <option value="100">High (100)</option>
            <option value="200">Critical (200)</option>
            <option value="10">Low (10)</option>
          </select>
        </div>
        <div class="form-group">
          <label for="form-max-attempts">Max Retry Attempts</label>
          <input type="number" id="form-max-attempts" value="3" min="1" max="10">
        </div>
        <div class="form-group">
          <label for="form-payload">Payload (JSON or Text)</label>
          <textarea id="form-payload">{"recipient": "user@example.com", "action": "deliver"}</textarea>
        </div>
        <div class="modal-actions-wrap" style="display:flex; justify-content:flex-end; gap:0.75rem; margin-top:1.5rem;">
          <button type="button" class="btn btn-secondary" onclick="closeSubmitModal()">Cancel</button>
          <button type="submit" class="btn btn-primary" id="btn-submit-action">Schedule Job</button>
        </div>
      </form>
    </div>
  </div>

  <!-- Job Details Modal -->
  <div class="modal-overlay" id="details-modal" role="dialog" aria-modal="true" aria-labelledby="details-title">
    <div class="modal-box">
      <div class="modal-header">
        <div>
          <div class="modal-title" id="details-title">Job Details</div>
          <div style="font-family:var(--font-mono); font-size:0.78rem; color:var(--text-muted); margin-top:0.25rem; word-break:break-all;" id="details-subtitle"></div>
        </div>
        <button class="modal-close" onclick="closeDetailsModal()" aria-label="Close job details modal">&times;</button>
      </div>

      <div id="details-error-container" style="display:none;" class="error-banner">
        <strong>Failure Diagnostics:</strong>
        <div id="details-error-text" style="margin-top:0.35rem; font-family:var(--font-mono); font-size:0.8rem;"></div>
      </div>

      <div class="meta-grid">
        <div class="meta-item">
          <span>Status</span>
          <span id="details-status">—</span>
        </div>
        <div class="meta-item">
          <span>Priority</span>
          <span id="details-priority">—</span>
        </div>
        <div class="meta-item">
          <span>Attempts</span>
          <span id="details-attempts">—</span>
        </div>
        <div class="meta-item">
          <span>Created / Scheduled</span>
          <span id="details-scheduled">—</span>
        </div>
      </div>

      <div class="form-group">
        <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.25rem;">
          <label for="details-payload">PAYLOAD</label>
          <button type="button" class="btn btn-secondary btn-xs" id="btn-copy-payload" onclick="copyPayload()">
            <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path></svg>
            <span>Copy</span>
          </button>
        </div>
        <pre class="payload-pre" id="details-payload"></pre>
      </div>

      <div class="modal-actions-wrap" style="display:flex; justify-content:space-between; align-items:center; margin-top:1.5rem; gap:0.5rem; flex-wrap:wrap;">
        <div id="details-actions" style="display:flex; gap:0.5rem; flex-wrap:wrap;"></div>
        <button type="button" class="btn btn-secondary" onclick="closeDetailsModal()">Close</button>
      </div>
    </div>
  </div>

  <!-- API Key Modal -->
  <div class="modal-overlay" id="apikey-modal" role="dialog" aria-modal="true" aria-labelledby="apikey-title">
    <div class="modal-box">
      <div class="modal-header">
        <div class="modal-title" id="apikey-title">API Authentication Key</div>
        <button class="modal-close" onclick="closeApiKeyModal()" aria-label="Close API key settings modal">&times;</button>
      </div>
      <p style="font-size:0.825rem; color:var(--text-muted); margin-bottom:1.25rem; line-height:1.5;">
        If TaskMesh was started with <code>TASKMESH_API_KEY</code>, provide your secret token below to authorize mutating requests (Schedule, Cancel, Retry, Delete).
      </p>
      <div class="form-group">
        <label for="input-api-key">X-API-Key / Bearer Token</label>
        <div style="position:relative;">
          <input type="password" id="input-api-key" placeholder="Enter API key token..." style="padding-right:2.5rem;">
          <button type="button" id="toggle-key-visibility" onclick="toggleKeyVisibility()" style="position:absolute; right:0.5rem; top:50%; transform:translateY(-50%); background:transparent; border:none; color:var(--text-muted); cursor:pointer; padding:0.25rem;" aria-label="Toggle key visibility">
            <svg id="eye-icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle></svg>
          </button>
        </div>
      </div>
      <div class="form-group" style="display:flex; align-items:center; gap:0.5rem; margin-top:0.75rem;">
        <input type="checkbox" id="check-remember-key" style="width:auto; cursor:pointer;">
        <label for="check-remember-key" style="margin:0; font-size:0.8rem; color:var(--text-secondary); cursor:pointer;">
          Remember across browser restarts (Local Storage). Unchecked = session only.
        </label>
      </div>
      <div class="modal-actions-wrap" style="display:flex; justify-content:space-between; margin-top:1.5rem; gap:0.75rem; flex-wrap:wrap;">
        <button type="button" class="btn btn-danger btn-xs" onclick="clearApiKey()">Clear Saved Key</button>
        <div style="display:flex; gap:0.5rem;">
          <button type="button" class="btn btn-secondary" onclick="closeApiKeyModal()">Cancel</button>
          <button type="button" class="btn btn-primary" onclick="saveApiKey()">Save Key</button>
        </div>
      </div>
    </div>
  </div>

  <!-- Delete Confirmation Modal (Repair: Destructive actions require confirmation) -->
  <div class="modal-overlay" id="delete-confirm-modal" role="dialog" aria-modal="true" aria-labelledby="delete-confirm-title">
    <div class="modal-box" style="max-width:460px;">
      <div class="modal-header">
        <div class="modal-title" id="delete-confirm-title" style="color:#f87171; display:flex; align-items:center; gap:0.5rem;">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="7.86 2 16.14 2 22 7.86 22 16.14 16.14 22 7.86 22 2 16.14 2 7.86 7.86 2"></polygon><line x1="12" y1="8" x2="12" y2="12"></line><line x1="12" y1="16" x2="12.01" y2="16"></line></svg>
          <span>Confirm Job Deletion</span>
        </div>
        <button class="modal-close" onclick="closeDeleteModal()" aria-label="Close delete confirmation modal">&times;</button>
      </div>
      <p style="font-size:0.85rem; color:var(--text-secondary); margin-bottom:1rem; line-height:1.5;">
        Are you sure you want to permanently delete this job record?
      </p>
      <div style="background:rgba(0,0,0,0.3); border:1px solid var(--border-color); border-radius:var(--radius-sm); padding:0.75rem 1rem; margin-bottom:1.25rem;">
        <span style="font-size:0.75rem; color:var(--text-muted); display:block; margin-bottom:0.25rem;">JOB ID:</span>
        <span id="delete-confirm-job-id" style="font-family:var(--font-mono); font-size:0.85rem; color:#fca5a5; word-break:break-all; font-weight:600;"></span>
      </div>
      <p style="font-size:0.78rem; color:var(--text-muted); margin-bottom:1.5rem;">
        This action cannot be undone. Active running jobs cannot be deleted until they settle.
      </p>
      <div class="modal-actions-wrap" style="display:flex; justify-content:flex-end; gap:0.75rem;">
        <button type="button" class="btn btn-secondary" onclick="closeDeleteModal()">Cancel</button>
        <button type="button" class="btn btn-danger-solid" id="btn-confirm-delete" onclick="executeDeleteJob()">Delete Permanently</button>
      </div>
    </div>
  </div>

  <!-- Toast Notification Container -->
  <div id="toast-container" aria-live="polite" aria-atomic="true"></div>

  <script>
    // State management
    let currentFilter = null;
    let allJobs = [];
    let currentDetailsJob = null;
    let pendingDeleteJobId = null;
    let isRequestBusy = false;

    // API Key helpers
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

    // Toast Notifications
    function showToast(msg, type = 'info') {
      const container = document.getElementById('toast-container');
      const toast = document.createElement('div');
      toast.className = `toast toast-${type}`;

      const textSpan = document.createElement('span');
      textSpan.innerText = msg;
      toast.appendChild(textSpan);

      const closeBtn = document.createElement('button');
      closeBtn.className = 'toast-close';
      closeBtn.innerHTML = '&times;';
      closeBtn.setAttribute('aria-label', 'Dismiss notification');
      closeBtn.onclick = () => removeToast(toast);
      toast.appendChild(closeBtn);

      container.appendChild(toast);

      // Automatically prune older toasts if more than 5
      while (container.children.length > 5) {
        removeToast(container.firstElementChild);
      }

      setTimeout(() => {
        removeToast(toast);
      }, 4000);
    }

    function removeToast(toast) {
      if (!toast || !toast.parentNode) return;
      toast.style.opacity = '0';
      toast.style.transform = 'translateX(50px)';
      toast.style.transition = 'all 0.25s ease';
      setTimeout(() => {
        if (toast.parentNode) toast.remove();
      }, 250);
    }

    // Mobile navigation toggle
    function toggleMobileNav() {
      const actions = document.getElementById('header-actions');
      const btn = document.getElementById('mobile-menu-btn');
      const isOpen = actions.classList.toggle('mobile-open');
      if (btn) btn.setAttribute('aria-expanded', isOpen ? 'true' : 'false');
    }

    // Modal helpers
    function closeAllModals() {
      document.querySelectorAll('.modal-overlay').forEach(modal => modal.classList.remove('active'));
    }

    // Global listeners for escape key and backdrop click
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') {
        closeAllModals();
      }
    });

    document.querySelectorAll('.modal-overlay').forEach(overlay => {
      overlay.addEventListener('click', (e) => {
        if (e.target === overlay) {
          overlay.classList.remove('active');
        }
      });
    });

    // API Key Modal
    function updateApiKeyIndicator() {
      const key = getStoredApiKey();
      const btnText = document.getElementById('api-key-status-text');
      if (key) {
        const isPersistent = isKeyInLocalStorage();
        btnText.innerHTML = `<span style="color:#34d399; font-size:0.7rem; margin-right:2px;">●</span> ${isPersistent ? 'Key (Saved)' : 'Key (Session)'}`;
      } else {
        btnText.innerText = 'API Key';
      }
    }

    function openApiKeyModal() {
      document.getElementById('input-api-key').value = getStoredApiKey();
      document.getElementById('check-remember-key').checked = isKeyInLocalStorage();
      document.getElementById('apikey-modal').classList.add('active');
      const actions = document.getElementById('header-actions');
      if (actions) actions.classList.remove('mobile-open');
    }

    function closeApiKeyModal() {
      document.getElementById('apikey-modal').classList.remove('active');
    }

    function toggleKeyVisibility() {
      const input = document.getElementById('input-api-key');
      const eyeIcon = document.getElementById('eye-icon');
      if (input.type === 'password') {
        input.type = 'text';
        eyeIcon.innerHTML = '<path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"></path><line x1="1" y1="1" x2="23" y2="23"></line>';
      } else {
        input.type = 'password';
        eyeIcon.innerHTML = '<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle>';
      }
    }

    function saveApiKey() {
      const key = document.getElementById('input-api-key').value.trim();
      const remember = document.getElementById('check-remember-key').checked;
      if (key) {
        if (remember) {
          localStorage.setItem('taskmesh_api_key', key);
          sessionStorage.removeItem('taskmesh_api_key');
          showToast('API Key saved to persistent storage', 'success');
        } else {
          sessionStorage.setItem('taskmesh_api_key', key);
          localStorage.removeItem('taskmesh_api_key');
          showToast('API Key saved for current session', 'success');
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
      showToast('API Key cleared from storage', 'info');
      fetchData();
    }

    // Submit Job Modal
    function openSubmitModal() {
      document.getElementById('submit-modal').classList.add('active');
      const actions = document.getElementById('header-actions');
      if (actions) actions.classList.remove('mobile-open');
    }

    function closeSubmitModal() {
      document.getElementById('submit-modal').classList.remove('active');
    }

    function closeDetailsModal() {
      document.getElementById('details-modal').classList.remove('active');
    }

    // Delete Confirmation Modal
    function openDeleteModal(id) {
      pendingDeleteJobId = id;
      document.getElementById('delete-confirm-job-id').innerText = id;
      document.getElementById('delete-confirm-modal').classList.add('active');
    }

    function closeDeleteModal() {
      pendingDeleteJobId = null;
      document.getElementById('delete-confirm-modal').classList.remove('active');
    }

    // Form preset helper
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

    // Filter and search
    function setFilter(status, el) {
      currentFilter = status;
      document.querySelectorAll('.filter-pill').forEach(b => {
        b.classList.remove('active');
        b.setAttribute('aria-selected', 'false');
      });
      el.classList.add('active');
      el.setAttribute('aria-selected', 'true');
      filterAndRender();
    }

    function onSearchInput() {
      const val = document.getElementById('search-input').value;
      const clearBtn = document.getElementById('search-clear-btn');
      clearBtn.style.display = val ? 'flex' : 'none';
      filterAndRender();
    }

    function clearSearch() {
      document.getElementById('search-input').value = '';
      document.getElementById('search-clear-btn').style.display = 'none';
      filterAndRender();
    }

    // Timestamp formatting utility
    function formatTimestamp(epochMs) {
      if (!epochMs) return '—';
      const d = new Date(Number(epochMs));
      if (isNaN(d.getTime())) return '—';
      const yyyy = d.getFullYear();
      const mm = String(d.getMonth() + 1).padStart(2, '0');
      const dd = String(d.getDate()).padStart(2, '0');
      const time = d.toLocaleTimeString();
      return `${yyyy}-${mm}-${dd} ${time}`;
    }

    // Poll data from TaskMesh Server
    async function fetchData() {
      const icon = document.getElementById('refresh-icon');
      if (icon) icon.style.animation = 'spin 0.6s linear infinite';

      try {
        const [mRes, jRes] = await Promise.all([
          fetch('/api/v1/metrics', { headers: getHeaders() }),
          fetch('/api/v1/jobs?limit=200', { headers: getHeaders() })
        ]);

        if (mRes.ok) {
          const m = await mRes.json();
          document.getElementById('val-submitted').innerText = m.submitted || 0;
          document.getElementById('val-queued').innerText = m.queued || 0;
          document.getElementById('val-running').innerText = m.running || 0;
          document.getElementById('val-completed').innerText = m.completed || 0;
          document.getElementById('val-failed').innerText = m.failed || 0;
          document.getElementById('val-cancelled').innerText = m.cancelled || 0;

          document.getElementById('engine-status').innerText = 'Engine Healthy';
          document.getElementById('engine-pulse-dot').style.background = 'var(--success)';
        } else if (mRes.status === 401) {
          document.getElementById('engine-status').innerText = 'Auth Required';
        }

        if (jRes.ok) {
          const data = await jRes.json();
          allJobs = data.jobs || [];
          filterAndRender();
        } else if (jRes.status === 401) {
          showToast('Authentication required. Click "API Key" to enter token.', 'warning');
        }
      } catch (err) {
        console.error('TaskMesh poll error:', err);
        document.getElementById('engine-status').innerText = 'Connection Lost';
        document.getElementById('engine-pulse-dot').style.background = 'var(--danger)';
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
        tbody.innerHTML = `
          <tr>
            <td colspan="7">
              <div class="empty-state">
                <svg viewBox="0 0 24 24" fill="none" stroke-width="1.5"><rect x="2" y="7" width="20" height="14" rx="2" ry="2"></rect><path d="M16 21V5a2 2 0 0 0-2-2h-4a2 2 0 0 0-2 2v16"></path></svg>
                <div class="empty-state-title">No jobs found</div>
                <div class="empty-state-desc">No tasks match your current search or filter criteria.</div>
                <button class="btn btn-secondary btn-xs" onclick="resetFilters()">Reset Filters</button>
              </div>
            </td>
          </tr>
        `;
        return;
      }

      tbody.innerHTML = jobs.map(j => {
        const canCancel = (j.status === 'Queued' || j.status === 'Running');
        const canRetry = (j.status === 'Failed' || j.status === 'Cancelled');
        const canDelete = (j.status === 'Completed' || j.status === 'Failed' || j.status === 'Cancelled');
        const prioName = j.priority_name ? j.priority_name.split('(')[0].trim() : 'Normal';
        const hasErr = Boolean(j.error_details);

        return `
          <tr>
            <td data-label="Priority">
              <div class="mobile-card-row">
                <span class="priority-badge priority-${escapeHtml(prioName)}">${escapeHtml(j.priority_name || 'Normal')}</span>
                <span class="status-badge status-${escapeHtml(j.status)}" style="display:none;" id="mob-status-${escapeHtml(j.id)}">${escapeHtml(j.status)}</span>
              </div>
            </td>
            <td data-label="Job ID">
              <strong class="mobile-card-id" style="cursor:pointer;" onclick="openDetails('${escapeHtml(j.id)}')" title="View Job Details">
                ${escapeHtml(j.id)}
              </strong>
            </td>
            <td data-label="Type">
              <span style="font-weight:500;">${escapeHtml(j.type)}</span>
            </td>
            <td data-label="Payload">
              <span class="code-text" title="${escapeHtml(j.payload)}">${escapeHtml(j.payload)}</span>
              ${hasErr ? `<span class="error-snippet" title="${escapeHtml(j.error_details)}">⚠ ${escapeHtml(j.error_details)}</span>` : ''}
            </td>
            <td data-label="Status">
              <span class="status-badge status-${escapeHtml(j.status)}">${escapeHtml(j.status)}</span>
            </td>
            <td data-label="Attempts">
              <span style="font-family:var(--font-mono); font-size:0.8rem;">${j.attempt_count} / ${j.max_attempts}</span>
            </td>
            <td data-label="Actions">
              <div class="mobile-card-actions">
                <button class="btn btn-secondary btn-xs" onclick="openDetails('${escapeHtml(j.id)}')" aria-label="Inspect job ${escapeHtml(j.id)}">Inspect</button>
                ${canCancel ? `<button class="btn btn-warning btn-xs" id="btn-cancel-${escapeHtml(j.id)}" onclick="cancelJob('${escapeHtml(j.id)}')" aria-label="Cancel job ${escapeHtml(j.id)}">Cancel</button>` : ''}
                ${canRetry ? `<button class="btn btn-primary btn-xs" id="btn-retry-${escapeHtml(j.id)}" onclick="retryJob('${escapeHtml(j.id)}')" aria-label="Retry job ${escapeHtml(j.id)}">Retry</button>` : ''}
                ${canDelete ? `<button class="btn btn-danger btn-xs" id="btn-del-${escapeHtml(j.id)}" onclick="openDeleteModal('${escapeHtml(j.id)}')" aria-label="Delete job ${escapeHtml(j.id)}">Delete</button>` : ''}
              </div>
            </td>
          </tr>
        `;
      }).join('');
    }

    function resetFilters() {
      clearSearch();
      const allPill = document.getElementById('filter-all');
      if (allPill) setFilter(null, allPill);
    }

    // Inspect Job Details Modal
    function openDetails(id) {
      const job = allJobs.find(j => j.id === id);
      if (!job) return;
      currentDetailsJob = job;

      document.getElementById('details-title').innerText = `${job.type} Task`;
      document.getElementById('details-subtitle').innerText = `ID: ${job.id}`;

      const prioName = job.priority_name ? job.priority_name.split('(')[0].trim() : 'Normal';
      document.getElementById('details-priority').innerHTML = `<span class="priority-badge priority-${escapeHtml(prioName)}">${escapeHtml(job.priority_name || 'Normal')}</span>`;
      document.getElementById('details-status').innerHTML = `<span class="status-badge status-${escapeHtml(job.status)}">${escapeHtml(job.status)}</span>`;
      document.getElementById('details-attempts').innerText = `${job.attempt_count} of ${job.max_attempts}`;

      // Fixed: correctly inspect created_at_ms or created_at_epoch_ms
      const createdEpoch = job.created_at_ms || job.created_at_epoch_ms;
      document.getElementById('details-scheduled').innerText = formatTimestamp(createdEpoch);

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
        btn.onclick = () => { openDeleteModal(job.id); closeDetailsModal(); };
        actionsDiv.appendChild(btn);
      }

      document.getElementById('details-modal').classList.add('active');
    }

    function copyPayload() {
      if (currentDetailsJob && currentDetailsJob.payload) {
        navigator.clipboard.writeText(currentDetailsJob.payload).then(() => {
          showToast('Payload copied to clipboard', 'success');
        }).catch(() => {
          showToast('Failed to copy payload', 'error');
        });
      }
    }

    // Submit Job
    async function submitJob(e) {
      e.preventDefault();
      if (isRequestBusy) return;

      const submitBtn = document.getElementById('btn-submit-action');
      const origText = submitBtn.innerText;
      isRequestBusy = true;
      submitBtn.disabled = true;
      submitBtn.innerHTML = '<span class="spinner"></span> Scheduling...';

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
        const resData = await res.json().catch(() => ({}));
        if (res.ok) {
          closeSubmitModal();
          showToast('Job scheduled successfully', 'success');
          document.getElementById('job-form').reset();
          onJobTypeChanged();
          fetchData();
        } else if (res.status === 401) {
          showToast('Authentication failed: Missing or invalid API key', 'error');
          openApiKeyModal();
        } else if (res.status === 409) {
          showToast(`Job ID conflict: ${resData.error || 'A job with this ID already exists'}`, 'error');
        } else {
          showToast(`Submit failed: ${resData.error || 'Server rejected request'}`, 'error');
        }
      } catch (err) {
        showToast('Request failed: ' + err.message, 'error');
      } finally {
        isRequestBusy = false;
        submitBtn.disabled = false;
        submitBtn.innerText = origText;
      }
    }

    // Cancel Job
    async function cancelJob(id) {
      if (isRequestBusy) return;
      isRequestBusy = true;

      const cancelBtn = document.getElementById(`btn-cancel-${id}`);
      if (cancelBtn) cancelBtn.disabled = true;

      try {
        const res = await fetch(`/api/v1/jobs/${encodeURIComponent(id)}/cancel`, {
          method: 'POST',
          headers: getHeaders()
        });
        const resData = await res.json().catch(() => ({}));
        if (res.ok) {
          showToast(`Job ${id} cancelled successfully`, 'info');
          fetchData();
        } else if (res.status === 401) {
          showToast('Authentication failed: Missing or invalid API key', 'error');
          openApiKeyModal();
        } else if (res.status === 409) {
          showToast(`Cannot cancel: ${resData.error || 'Job already in terminal state'}`, 'warning');
          fetchData();
        } else {
          showToast(`Cancel failed: ${resData.error || 'Request rejected'}`, 'error');
        }
      } catch (err) {
        showToast('Request failed: ' + err.message, 'error');
      } finally {
        isRequestBusy = false;
        if (cancelBtn) cancelBtn.disabled = false;
      }
    }

    // Retry Job
    async function retryJob(id) {
      if (isRequestBusy) return;
      isRequestBusy = true;

      const retryBtn = document.getElementById(`btn-retry-${id}`);
      if (retryBtn) retryBtn.disabled = true;

      try {
        const res = await fetch(`/api/v1/jobs/${encodeURIComponent(id)}/retry`, {
          method: 'POST',
          headers: getHeaders()
        });
        const resData = await res.json().catch(() => ({}));
        if (res.ok) {
          showToast(`Job ${id} re-queued for retry`, 'success');
          fetchData();
        } else if (res.status === 401) {
          showToast('Authentication failed: Missing or invalid API key', 'error');
          openApiKeyModal();
        } else if (res.status === 409) {
          showToast(`Retry rejected: ${resData.error || 'Job is not in retryable state'}`, 'warning');
          fetchData();
        } else {
          showToast(`Retry failed: ${resData.error || 'Request rejected'}`, 'error');
        }
      } catch (err) {
        showToast('Request failed: ' + err.message, 'error');
      } finally {
        isRequestBusy = false;
        if (retryBtn) retryBtn.disabled = false;
      }
    }

    // Execute Delete Job (Invoked only after confirmation)
    async function executeDeleteJob() {
      if (!pendingDeleteJobId || isRequestBusy) return;
      const id = pendingDeleteJobId;
      isRequestBusy = true;

      const confirmBtn = document.getElementById('btn-confirm-delete');
      const origText = confirmBtn.innerText;
      confirmBtn.disabled = true;
      confirmBtn.innerHTML = '<span class="spinner"></span> Deleting...';

      try {
        const res = await fetch(`/api/v1/jobs/${encodeURIComponent(id)}`, {
          method: 'DELETE',
          headers: getHeaders()
        });
        const resData = await res.json().catch(() => ({}));
        if (res.ok) {
          showToast(`Job ${id} permanently deleted`, 'info');
          closeDeleteModal();
          fetchData();
        } else if (res.status === 401) {
          showToast('Authentication failed: Missing or invalid API key', 'error');
          closeDeleteModal();
          openApiKeyModal();
        } else if (res.status === 409) {
          showToast(`Delete rejected: ${resData.error || 'Running jobs cannot be deleted'}`, 'warning');
          closeDeleteModal();
          fetchData();
        } else {
          showToast(`Delete failed: ${resData.error || 'Request rejected'}`, 'error');
        }
      } catch (err) {
        showToast('Request failed: ' + err.message, 'error');
      } finally {
        isRequestBusy = false;
        confirmBtn.disabled = false;
        confirmBtn.innerText = origText;
      }
    }

    // Safe HTML Escaping
    function escapeHtml(str) {
      if (!str) return '';
      return String(str).replace(/[&<>"']/g, s => ({
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#39;'
      })[s]);
    }

    // Auto-refresh interval (2 seconds)
    setInterval(() => {
      const toggle = document.getElementById('auto-refresh-toggle');
      if (toggle && toggle.checked) {
        fetchData();
      }
    }, 2000);

    // Initial setup on page load
    updateApiKeyIndicator();
    fetchData();
  </script>
</body>
</html>)rawhtml";
}

} // namespace taskmesh
