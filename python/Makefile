PY ?= python3
VENV ?= .venv
PIP = $(VENV)/bin/pip
PYBIN = $(VENV)/bin/python

.PHONY: help venv install dev orchestrate bench train test reset clean

help:
	@echo "make venv         - create virtual environment"
	@echo "make install      - install memogent editable"
	@echo "make dev          - install dev deps"
	@echo "make orchestrate  - replay a synthetic trace with live KPI table"
	@echo "make bench        - run the full benchmark harness and emit JSON"
	@echo "make train        - train the LSTM predictor on LSApp"
	@echo "make test         - pytest suite"
	@echo "make reset        - wipe runtime state"

venv:
	$(PY) -m venv $(VENV)
	$(PIP) install --upgrade pip setuptools wheel

install: venv
	$(PIP) install -e .

dev: install
	$(PIP) install -e .[dev]

orchestrate:
	$(PYBIN) -m memogent.cli.main orchestrate

bench:
	$(PYBIN) -m memogent.cli.main bench

train:
	$(PYBIN) -m memogent.cli.main train

test:
	$(PYBIN) -m pytest tests -v

reset:
	rm -rf data bench_results

clean:
	find . -type d -name '__pycache__' -exec rm -rf {} +
	rm -rf .ruff_cache .pytest_cache build dist *.egg-info
