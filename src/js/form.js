/**
 * 表单弹窗：通用字段渲染 / 校验 / 二次确认 / 多项选择。
 * 基于 ui.js 的 openModal，不重复实现遮罩与快捷键逻辑。
 */

import { icon } from './icons.js';
import { esc, openModal } from './ui.js';

const BTN_KIND = {
  primary: 'btn--primary',
  secondary: 'btn--secondary',
  ghost: 'btn--ghost',
  danger: 'btn--danger',
};

function footerHtml(footer) {
  return `<div class="modal-foot">${footer
    .map(
      (f) =>
        `<button type="button" class="btn ${BTN_KIND[f.kind] || BTN_KIND.secondary}" data-act="${esc(f.act)}">${
          f.iconName ? icon(f.iconName, 16) : ''
        }<span>${esc(f.label)}</span></button>`
    )
    .join('')}</div>`;
}

function controlHtml(field, value) {
  const v = value === undefined || value === null ? '' : value;
  const ph = field.placeholder ? ` placeholder="${esc(field.placeholder)}"` : '';
  switch (field.type) {
    case 'select':
      return `<select class="input" id="f-${esc(field.name)}">
          ${field.options
            .map((o) => `<option value="${esc(o.value)}" ${String(o.value) === String(v) ? 'selected' : ''}>${esc(o.label)}</option>`)
            .join('')}
        </select>`;
    case 'textarea':
      return `<textarea class="input input--area" id="f-${esc(field.name)}"${ph}>${esc(v)}</textarea>`;
    case 'number':
      return `<input class="input" type="number" step="1" id="f-${esc(field.name)}" value="${esc(v)}"${ph} />`;
    case 'checks': {
      const selected = Array.isArray(v) ? v.map(String) : [];
      return `<div class="checks">${field.options
        .map(
          (o) =>
            `<label class="check-item"><input type="checkbox" data-check="${esc(field.name)}" value="${esc(o.value)}" ${
              selected.includes(String(o.value)) ? 'checked' : ''
            } /><span>${esc(o.label)}</span></label>`
        )
        .join('')}</div>`;
    }
    default:
      return `<input class="input" type="text" id="f-${esc(field.name)}" value="${esc(v)}"${ph} />`;
  }
}

function collectValues(root, fields) {
  const values = {};
  for (const field of fields) {
    if (field.type === 'checks') {
      values[field.name] = Array.from(root.querySelectorAll(`input[data-check="${field.name}"]:checked`)).map((n) => n.value);
      continue;
    }
    const node = root.querySelector(`#f-${field.name}`);
    if (!node) {
      values[field.name] = field.type === 'number' ? null : '';
      continue;
    }
    if (field.type === 'number') {
      const raw = String(node.value).trim();
      const parsed = Number(raw);
      values[field.name] = raw === '' || !Number.isFinite(parsed) ? null : parsed;
    } else {
      values[field.name] = String(node.value).trim();
    }
  }
  return values;
}

function validateValues(values, fields) {
  const errors = [];
  for (const field of fields) {
    const v = values[field.name];
    if (field.required && (v === '' || v === null || v === undefined || (Array.isArray(v) && v.length === 0))) {
      errors.push({ name: field.name, message: `${field.label}不能为空` });
      continue;
    }
    if (field.type === 'number' && v !== null && v !== undefined) {
      if (!Number.isFinite(v)) errors.push({ name: field.name, message: `${field.label}必须是数字` });
      else if (field.min !== undefined && v < field.min) errors.push({ name: field.name, message: `${field.label}不能小于 ${field.min}` });
      else if (field.max !== undefined && v > field.max) errors.push({ name: field.name, message: `${field.label}不能大于 ${field.max}` });
    }
  }
  return errors;
}

/**
 * 表单弹窗。
 * @param fields [{name,label,type,value,required,placeholder,options,hint,min,max}]
 * @returns {Promise<object|null>} 提交值；取消 / ESC 返回 null
 */
export function formModal({ title, desc, fields, confirmText = '保存', danger = false, width, extraFooter = [] }) {
  return new Promise((resolve) => {
    let settled = false;
    const done = (value) => {
      if (settled) return;
      settled = true;
      resolve(value);
    };

    const body =
      (desc ? `<p class="modal-desc">${esc(desc)}</p>` : '') +
      `<div class="col">${fields
        .map(
          (f) => `<div class="field">
            <label class="field-label" for="f-${esc(f.name)}">${esc(f.label)}${f.required ? '<span class="req">*</span>' : ''}</label>
            ${controlHtml(f, f.value)}
            ${f.hint ? `<span class="field-hint">${esc(f.hint)}</span>` : ''}
            <span class="field-error" data-error="${esc(f.name)}"></span>
          </div>`
        )
        .join('')}</div>`;

    openModal({
      title,
      body,
      width,
      footer: [...extraFooter, { label: '取消', kind: 'secondary', act: 'cancel' }, { label: confirmText, kind: danger ? 'danger' : 'primary', act: 'ok' }],
      onClose: () => done(null),
      onAction: (act, _trigger, api) => {
        if (act === 'cancel') {
          api.close();
          return;
        }
        if (act !== 'ok') return;
        const values = collectValues(api.root, fields);
        const errors = validateValues(values, fields);
        api.root.querySelectorAll('.field-error').forEach((n) => (n.textContent = ''));
        api.root.querySelectorAll('.input').forEach((n) => n.classList.remove('is-invalid'));
        if (errors.length) {
          for (const err of errors) {
            const slot = api.root.querySelector(`[data-error="${err.name}"]`);
            if (slot) slot.textContent = err.message;
            const input = api.root.querySelector(`#f-${err.name}`);
            if (input) input.classList.add('is-invalid');
          }
          return;
        }
        done(values);
        api.close();
      },
    });
  });
}

/** 二次确认框 */
export function confirmBox(message, { title = '请确认', confirmText = '确定删除', danger = true, desc = '' } = {}) {
  return new Promise((resolve) => {
    let settled = false;
    const done = (value) => {
      if (settled) return;
      settled = true;
      resolve(value);
    };
    const body = `<div class="modal-icon-row">
        <div class="modal-danger-icon">${icon(danger ? 'alert' : 'info', 20)}</div>
        <div class="col">
          <span>${esc(message)}</span>
          ${desc ? `<span class="field-hint">${esc(desc)}</span>` : ''}
        </div>
      </div>`;
    openModal({
      title,
      body,
      width: 400,
      footer: [
        { label: '取消', kind: 'secondary', act: 'cancel' },
        { label: confirmText, kind: danger ? 'danger' : 'primary', act: 'ok' },
      ],
      onClose: () => done(false),
      onAction: (act, _t, api) => {
        done(act === 'ok');
        api.close();
      },
    });
  });
}

/**
 * 多项选择框（取代二值确认，用于「替换 / 合并 / 取消」这类三选一）。
 * @param options [{label, value, kind, iconName, desc}]
 * @returns {Promise<*>} 选中项的 value，取消返回 null
 */
export function choiceModal({ title, message, options = [], width = 440 }) {
  return new Promise((resolve) => {
    let settled = false;
    const done = (value) => {
      if (settled) return;
      settled = true;
      resolve(value);
    };
    const body = `<p class="modal-desc">${esc(message)}</p>
      <div class="col">
        ${options
          .map(
            (o) => `<button type="button" class="btn btn--choice ${BTN_KIND[o.kind] || BTN_KIND.secondary}" data-act="choice" data-choice="${esc(o.value)}">
              ${o.iconName ? icon(o.iconName, 16) : ''}
              <span class="btn-choice-text">
                <span>${esc(o.label)}</span>
                ${o.desc ? `<span class="field-hint">${esc(o.desc)}</span>` : ''}
              </span>
            </button>`
          )
          .join('')}
      </div>`;
    openModal({
      title,
      body,
      width,
      footer: [{ label: '取消', kind: 'ghost', act: 'cancel' }],
      onClose: () => done(null),
      onAction: (act, trigger, api) => {
        done(act === 'choice' && trigger ? trigger.dataset.choice : null);
        api.close();
      },
    });
  });
}

export function toOptions(items, labelKey, valueKey) {
  return items.map((i) => ({ label: i[labelKey], value: i[valueKey] }));
}
