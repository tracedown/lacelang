# Build and verify all lacelang spec artifacts.
#
# Targets:
#   make verify       — full check: grammar + schemas + .laceext + conformance vectors
#   make grammar-test — generate parser, run all grammar samples
#   make grammar      — generate Java parser only
#   make schemas      — compile all JSON schemas, verify error-codes registry
#   make laceext      — validate every .laceext file under ../extensions/
#   make vectors      — validate every conformance vector under ../testkit/vectors/
#   make tools-deps   — install tools/ Node dependencies
#   make clean        — remove build/ directory
#
# Requirements:
#   - Java (any JDK >= 11) on PATH for grammar tests
#   - Node.js on PATH for schemas/laceext checks
#   - ANTLR jar downloaded automatically into ./build on first grammar run

ANTLR_VERSION ?= 4.13.2
ANTLR_JAR     ?= ./build/antlr-$(ANTLR_VERSION).jar
ANTLR_URL     := https://www.antlr.org/download/antlr-$(ANTLR_VERSION)-complete.jar
GEN_DIR       := ./build/gen
GRAMMAR       := lacelang.g4
POS_DIR       := grammar-tests/positive
NEG_DIR       := grammar-tests/negative
TOOLS_DIR     := tools

ifeq ($(OS),Windows_NT)
  CP_SEP := ;
else
  CP_SEP := :
endif

.PHONY: verify grammar-test grammar schemas laceext vectors tools-deps clean fetch-antlr

verify: grammar-test schemas laceext vectors
	@echo ""
	@echo "── All spec verifications passed. ──"

# ── Grammar ────────────────────────────────────────────────────────

grammar-test: grammar
	@echo "── Grammar — positive cases (must parse) ──"
	@cd $(GEN_DIR) && for f in ../../$(POS_DIR)/*.lace; do \
	  name=$$(basename $$f); \
	  out=$$(java -cp ".$(CP_SEP)../../$(ANTLR_JAR)" org.antlr.v4.gui.TestRig lacelang script "$$f" 2>&1); \
	  if [ -n "$$out" ]; then echo "FAIL: $$name"; echo "$$out"; exit 1; \
	  else echo "  ok: $$name"; fi; \
	done
	@echo "── Grammar — negative cases (must error) ──"
	@cd $(GEN_DIR) && for f in ../../$(NEG_DIR)/*.lace; do \
	  name=$$(basename $$f); \
	  out=$$(java -cp ".$(CP_SEP)../../$(ANTLR_JAR)" org.antlr.v4.gui.TestRig lacelang script "$$f" 2>&1); \
	  if [ -z "$$out" ]; then echo "FAIL: $$name (expected parse error, got none)"; exit 1; \
	  else echo "  ok: $$name (rejected as expected)"; fi; \
	done

grammar: $(GEN_DIR)/lacelangParser.class

$(GEN_DIR)/lacelangParser.class: $(GRAMMAR) $(ANTLR_JAR)
	@mkdir -p $(GEN_DIR)
	java -jar $(ANTLR_JAR) -Dlanguage=Java -o $(GEN_DIR) $(GRAMMAR)
	cd $(GEN_DIR) && javac -cp "../../$(ANTLR_JAR)" *.java

$(ANTLR_JAR):
	@mkdir -p $(dir $(ANTLR_JAR))
	curl -fsSL -o $(ANTLR_JAR) $(ANTLR_URL)

# ── Schemas ────────────────────────────────────────────────────────

schemas: tools-deps
	@echo "── JSON schemas + error code registry ──"
	@cd $(TOOLS_DIR) && node check-schemas.js

# ── Extensions ─────────────────────────────────────────────────────

laceext: tools-deps
	@echo "── .laceext files ──"
	@cd $(TOOLS_DIR) && node check-laceext.js

vectors: tools-deps
	@echo "── conformance vectors ──"
	@cd $(TOOLS_DIR) && node check-vectors.js

# ── Tooling ────────────────────────────────────────────────────────

tools-deps: $(TOOLS_DIR)/node_modules/.installed

$(TOOLS_DIR)/node_modules/.installed: $(TOOLS_DIR)/package.json
	cd $(TOOLS_DIR) && npm install --silent
	@touch $@

clean:
	rm -rf ./build $(TOOLS_DIR)/node_modules $(TOOLS_DIR)/package-lock.json
