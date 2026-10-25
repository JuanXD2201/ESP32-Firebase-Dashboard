FROM python:3.12-slim
ENV PYTHONUNBUFFERED=1 PORT=8080
WORKDIR /srv
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY app app
COPY wsgi.py .
RUN useradd -r app && chown -R app /srv
USER app
# Cloud Run inyecta PORT; las credenciales vienen de la cuenta de servicio del servicio (ADC).
CMD exec gunicorn --bind :$PORT --workers 1 --threads 4 --timeout 30 wsgi:app
