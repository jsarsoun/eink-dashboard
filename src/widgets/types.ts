export interface Widget<T = unknown> {
  /** Unique key used in the dashboard JSON response (e.g. "weather", "calendar") */
  id: string;

  /**
   * Fetch this widget's data. Reads its own config from the DB directly.
   * Throws on failure — the dashboard route uses Promise.allSettled to handle this gracefully.
   */
  fetch(): Promise<T>;
}
