document.addEventListener('mouseover', function(e) {
    let target = e.target.closest('.control[title], .dock-item[title]');
    if (target) {
        let title = target.getAttribute('title');
        target.setAttribute('data-title', title);
        target.removeAttribute('title');
    }
});

document.addEventListener('mouseout', function(e) {
    let target = e.target.closest('.control[data-title], .dock-item[data-title]');
    if (target) {
        if (!e.relatedTarget || !target.contains(e.relatedTarget)) {
            let title = target.getAttribute('data-title');
            target.setAttribute('title', title);
            target.removeAttribute('data-title');
        }
    }
});
