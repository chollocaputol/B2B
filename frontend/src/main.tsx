import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import StudyAccess from './StudyAccess';
import './styles.css';
ReactDOM.createRoot(document.getElementById('root')!).render(<React.StrictMode>{new URLSearchParams(location.search).get('study')==='1'?<StudyAccess/>:<App/>}</React.StrictMode>);
