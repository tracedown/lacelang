FROM python:3.12-slim AS build

WORKDIR /app
COPY pygments-lace/ pygments-lace/
COPY mkdocs.yml .
COPY docs/ docs/
COPY overrides/ overrides/

RUN pip install --no-cache-dir "mkdocs>=1.6,<2" "mkdocs-material>=9.6,<10" ./pygments-lace && \
    mkdocs build --strict

FROM nginx:alpine
RUN rm /etc/nginx/conf.d/default.conf
COPY nginx.conf /etc/nginx/templates/default.conf.template
COPY --from=build /app/site /usr/share/nginx/html
