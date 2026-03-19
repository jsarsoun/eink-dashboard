import ConfigForm from './components/ConfigForm.js';

export default function App() {
  return (
    <div className="min-h-screen bg-neutral-100 text-neutral-900 font-sans">
      <header className="bg-white border-b border-neutral-200">
        <div className="max-w-xl mx-auto px-4 h-16 flex items-center">
          <h1 className="text-xl font-semibold tracking-tight">E-Ink Dashboard</h1>
        </div>
      </header>
      <main className="max-w-xl mx-auto px-4 py-8">
        <div className="bg-white rounded-xl shadow-sm border border-neutral-200 p-6">
          <ConfigForm />
        </div>
      </main>
    </div>
  );
}
